/***
exocaster -- audio streaming helper
server.cc -- main server

MIT License 

Copyright (c) 2024 ziplantil

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

***/

#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

#include "decoder/decoder.hh"
#include "encoder/encoder.hh"
#include "log.hh"
#include "queue/commandqueue.hh"
#include "pcmbuffer.hh"
#include "publisher.hh"
#include "registry.hh"
#include "server.hh"
#include "serverconfig.hh"
#include "version.hh"

namespace exo {

std::counting_semaphore<exo::MAX_BROCAS> brocaCounter(0);

exo::QuitStatus quitStatus = exo::QuitStatus::RUNNING;

struct ServerParameters {
    std::string configFilePath;
};

[[noreturn]] static void print_help() {
    std::cout << "exocaster: broadcasting middleman\n";
    std::cout << "      -?, --help          display help\n";
    std::cout << "      -c                  provide configuration path\n";
    std::cout << std::endl;
    std::exit(EXIT_SUCCESS);
}

[[noreturn]] static void print_version() {
    std::cout << "exocaster: broadcasting middleman\n";
    std::cout << "version " EXO_VERSION "\n";
    
    std::cout << "[supported read queues]";
    exo::printReadQueueOptions(std::cout);
    std::cout << std::endl;

    std::cout << "[supported write queues]";
    exo::printWriteQueueOptions(std::cout);
    std::cout << std::endl;

    std::cout << "[supported decoders]";
    exo::printDecoderOptions(std::cout);
    std::cout << std::endl;

    std::cout << "[supported encoders]";
    exo::printEncoderOptions(std::cout);
    std::cout << std::endl;

    std::cout << "[supported brocas]";
    exo::printBrocaOptions(std::cout);
    std::cout << std::endl;

    std::cout << std::endl;
    std::exit(EXIT_SUCCESS);
}

static exo::ServerParameters readParameters(int argc, char* argv[]) {
    bool acceptingFlags = true;
    bool hasC = false;
    std::string configFilePath = "config.json";

    for (int i = 1; i < argc; ++i) {
        char detectedConfig = 0;

        if (acceptingFlags && argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'c':
                    detectedConfig = argv[i][1];
                    break;
                case 'v':
                    print_version();
                    break;
                case '?':
                    print_help();
                    break;
                case '-':
                    if (!argv[i][2]) {
                        acceptingFlags = false;
                        break;
                    } else if (!std::strcmp(argv[i], "--help")) {
                        print_help();
                        break;
                    } else if (!std::strcmp(argv[i], "--version")) {
                        print_version();
                        break;
                    }
                    EXO_LOG("unrecognized option '%s' given, "
                            "exiting.", argv[i]);
                    std::exit(EXIT_FAILURE);
                default:
                    EXO_LOG("unrecognized option '-%c' given, "
                            "exiting.", argv[i][1]);
                    std::exit(EXIT_FAILURE);
            }
        }

        switch (detectedConfig) {
            case 0:
                EXO_LOG("ignoring positional parameter.");
                break;
            case 'c':
                if (hasC) {
                    EXO_LOG("got duplicate -c, ignoring latter.");
                } else if (i + 1 < argc) {
                    hasC = true;
                    configFilePath = argv[++i];
                } else {
                    EXO_LOG("no path provided for -c, exiting.");
                    std::exit(EXIT_FAILURE);
                }
                break;
        }
    }

    return exo::ServerParameters{
        .configFilePath = configFilePath
    };
}

static exo::ServerConfig readConfig(const exo::ServerParameters& params) {
    // read configuration file
    std::ifstream configFile(params.configFilePath, std::ios::in);
    if (!configFile) {
        EXO_LOG("cannot open configuration file, exiting: %s",
                std::strerror(errno));
        std::exit(EXIT_FAILURE);
    }

    try {
        auto configJson = exo::cfg::parseFromFile(configFile);
        return exo::ServerConfig::read(configJson);
    } catch (const std::exception& err) {
        EXO_LOG("configuration read error: %s", err.what());
        EXO_LOG("failed to read configuration, exiting.");
        std::exit(EXIT_FAILURE);
    }
}

using DecoderJobQueue = exo::JobQueue<std::shared_ptr<exo::PcmSplitter>>;

class Server {
    static constexpr unsigned JOB_QUEUE_SIZE = 8;
    static constexpr unsigned JOB_WORKER_COUNT = 2;

    exo::ServerConfig config_;
    exo::PcmFormat format_;
    std::thread readCommandsThread_;
    std::unique_ptr<exo::CommandQueue> commandQueue_;
    std::shared_ptr<exo::Publisher> publisher_;
    std::shared_ptr<exo::PcmSplitter> pcm_;
    std::unique_ptr<exo::DecoderJobQueue> jobs_;
    std::unordered_map<std::string, std::unique_ptr<exo::BaseDecoder>> cmd_;
    std::vector<std::unique_ptr<exo::BaseEncoder>> enc_;
    std::vector<std::unique_ptr<exo::BaseBroca>> broca_;
    std::string instanceId_;

    void readCommands_();

public:
    void init();
    void run();
    void close();

    Server(exo::ServerConfig&& config) : config_(config) {
        try{
            init();
        } catch (const std::exception& err) {
            EXO_LOG("server start error: %s", err.what());
            EXO_LOG("failed to start server, exiting.");
            std::exit(EXIT_FAILURE);
        }
    }
};

void Server::init() {
    EXO_LOG("allocating resources");
    instanceId_ = exo::UUID::uuid7().str();
    format_ = config_.pcmbuffer.pcmFormat();
    if (config_.outputs.empty()) {
        EXO_LOG("no encoders configured, will exit.");
        std::exit(EXIT_FAILURE);
    }
    publisher_ = std::make_unique<exo::Publisher>();
    pcm_ = exo::createPcmBuffers(config_.pcmbuffer, publisher_);
    jobs_ = std::make_unique<exo::DecoderJobQueue>(JOB_QUEUE_SIZE, pcm_);
    exo::registerCommands(cmd_, config_.commands, format_);
    exo::registerOutputs(enc_, broca_, *pcm_,
                         config_.outputs, config_.pcmbuffer, format_);
    commandQueue_ = std::make_unique<exo::CommandQueue>(
                exo::createReadQueue(config_.shell, instanceId_));
    for (const auto& publish: config_.publish)
        publisher_->addQueue(exo::createWriteQueue(
                    publish, instanceId_));
}

static auto startEncoder(const std::unique_ptr<exo::BaseEncoder>& encoder) {
    return std::thread([&encoder]() { encoder->run(); });
}

static auto startBroca(const std::unique_ptr<exo::BaseBroca>& broca) {
    return std::thread([&broca]() { broca->run(); });
}

static std::mutex quittingMutex_;
static std::condition_variable quittingCondition_;

bool shouldRun(exo::QuitStatus status) {
    return quitStatus < status;
}

void quit(exo::QuitStatus status) {
    if (status < quitStatus) return;

    std::unique_lock lock(quittingMutex_);
    quitStatus = status;
    lock.unlock();
    exo::quittingCondition_.notify_all();
}

[[maybe_unused]] static void waitForQuit(exo::QuitStatus status) {
    std::unique_lock lock(exo::quittingMutex_);
    exo::quittingCondition_.wait(lock, [status]() {
        return exo::quitStatus >= status;
    });
}

void Server::readCommands_() {
    EXO_LOG("now accepting commands");

    while (exo::shouldRun(exo::QuitStatus::NO_MORE_COMMANDS)) {
        auto command = commandQueue_->nextCommand();
        if (!exo::shouldRun(exo::QuitStatus::NO_MORE_COMMANDS))
            break;

        if (command.cmd == "quit") {
            exo::quit(exo::QuitStatus::NO_MORE_COMMANDS);
            break;
        }

        auto resolved = cmd_.find(command.cmd);
        if (resolved == cmd_.end()) {
            EXO_LOG("unknown command '%s', ignoring.", command.cmd.c_str());
            continue;
        }

        auto raw = std::make_shared<exo::ConfigObject>(std::move(command.raw));
        auto createdJob = resolved->second->createJob(command.param, raw);
        if (createdJob.has_value())
            jobs_->addJob(std::move(createdJob.value()));
    }
}

void Server::run() {
    EXO_LOG("starting exocaster " EXO_VERSION);
    std::vector<std::thread> encoders;
    for (const auto& encoder: enc_)
        encoders.push_back(exo::startEncoder(encoder));

    std::vector<std::thread> brocas;
    for (const auto& broca: broca_)
        brocas.push_back(exo::startBroca(broca));

    jobs_->start(JOB_WORKER_COUNT);
    publisher_->start();
    readCommandsThread_ = std::thread([this]() { this->readCommands_(); });
    exo::waitForQuit(exo::QuitStatus::NO_MORE_COMMANDS);

    if (exo::shouldRun(exo::QuitStatus::NO_MORE_JOBS))
        EXO_LOG("letting existing jobs finish...");
    jobs_->stop();
    exo::quit(exo::QuitStatus::NO_MORE_JOBS);
    pcm_->close();

    // wait for brocas to finish
    for (const auto& _: broca_)
        brocaCounter.acquire();

    exo::quit(exo::QuitStatus::NO_MORE_EVENTS);
    publisher_->close();

    exo::quit(exo::QuitStatus::QUITTING);

    for (auto& thread: encoders) thread.join();
    for (auto& thread: brocas) thread.join();
    // wait for publishers to finish
    publisher_->stop();
    // detach the command queue thread, it doesn't matter anymore
    readCommandsThread_.detach();
}

void Server::close() {
    commandQueue_->close();
}

static std::unique_ptr<exo::Server> server;

static void exitGracefullyOnSignal(int signal) {
    if (signal == SIGHUP)
        EXO_LOG("received SIGHUP, quitting.");
    if (signal == SIGINT)
        EXO_LOG("received SIGINT, quitting.");
    if (signal == SIGTERM)
        EXO_LOG("received SIGTERM, quitting.");
    quitStatus = exo::QuitStatus::QUITTING;
    server->close();
    exo::quittingCondition_.notify_all();
}

static void catchSignals() {
    struct sigaction act;

    std::memset(&act, 0, sizeof(struct sigaction));
    act.sa_flags = SA_RESETHAND;
    act.sa_handler = exitGracefullyOnSignal;
    sigaction(SIGHUP, &act, NULL);

    std::memset(&act, 0, sizeof(struct sigaction));
    act.sa_flags = SA_RESETHAND;
    act.sa_handler = exitGracefullyOnSignal;
    sigaction(SIGINT, &act, NULL);

    std::memset(&act, 0, sizeof(struct sigaction));
    act.sa_flags = SA_RESETHAND;
    act.sa_handler = exitGracefullyOnSignal;
    sigaction(SIGTERM, &act, NULL);
}

static void runServer(const exo::ServerParameters& params) {
    exo::ServerConfig config = exo::readConfig(params);
    server = std::make_unique<exo::Server>(std::move(config));
    catchSignals();
    server->run();
}

} // namespace exo

int main(int argc, char* argv[]) {
    exo::runServer(exo::readParameters(argc, argv));
    return EXIT_SUCCESS;
}

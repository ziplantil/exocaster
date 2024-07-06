
# ZeroMQ read queue

Type: `zeromq`

Reads JSON commands with a ZeroMQ PULL socket.

**Note that Exocaster is designed to receive commands only from sources**
**that can be 100% trusted.**

## Configuration

A JSON object with the following fields:

* `address` (required): The address to connect the socket to, as a string.

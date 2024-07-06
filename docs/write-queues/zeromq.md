
# ZeroMQ write queue

Type: `zeromq`

Publishes JSON messages with a ZeroMQ PUB socket.

## Configuration

A JSON object with the following fields:

* `address` (required): The address to bind the socket to, as a string.
* `topic` (optional): The topic to publish on, as a string.
* `topicId` (optional): A boolean value. If `true`, a unique ID, generated
  once every time Exocaster starts, is appended to the topic name. This can
  be used to identify multiple concurrent instances.

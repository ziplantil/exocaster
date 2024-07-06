
# HTTP POST write queue

Type: `httppost`

Publishes JSON messages as HTTP POST requests.

## Configuration

A JSON object with the following fields:

* `url` (required): The URL to make requests to, as a string.
* `headers` (optional): A JSON object specifying any additional headers
  to be sent with the request. The key is the header name and the value
  the header value.
* `instanceParameter` (optional): If specified, must be a string. The name
  of the query parameter added to the URL, with a value as a string that is
  generated randomly when Exocaster starts. Can be used by the target server
  to distinguish between multiple concurrently running instances.

## Notes

The POST request always carries the header `Content-Type: application/json` and
a response body consisting only of the event JSON.


# HTTP GET read queue

Type: `httpget`

Reads JSON commands by making HTTP GET requests.

**Note that Exocaster is designed to receive commands only from sources**
**that can be 100% trusted.**

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
* `cacheBustParameter` (optional): If specified, must be a string. The name
  of the query parameter added to the URL, with a value as a string that is
  generated randomly on every request. Intended to prevent the HTTP server
  from caching the response.

## Notes

The GET request always carries the header `Accept: application/json`. It
expects to receive the JSON object, and nothing more, as the response body.

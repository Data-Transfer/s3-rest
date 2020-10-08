#!/usr/bin/env python3

import sys
import logging
from http.server import BaseHTTPRequestHandler, HTTPServer
import requests
import typing as T
from urllib.parse import parse_qs
from urllib.parse import urlparse
import json
import re
import argparse

"""Minimal proxy http server, uses 'logging' module, forwards requests to
   remote endpoint.

   __author__: Ugo Varetto

   Usage: ./web-proxy.py <port> <endpoint>

   HTTPServer will be printing out a 'localhost - - [date] "Request" status -'
   at each request in addition to the data logged by the ProxyRequestHandler
   class.

   The web server accepts /__config/... URIs to remotely configure the
   environment, currently only /__config/endpoint=<endpoint> and
   /__config/download-chunk-size are supported
"""

# TODO:move all configuration parameters into _CONFIG dict and just call
# dict.update with content of JSON config request

_REMOTE_URL: str = ""
_DOWNLOAD_CHUNK_SIZE: int = 1 << 20
_MAX_LOG_CONTENT_LENGTH: int = 1 << 10
_FILTER_CONTENT = False
_MUTE = False

# NOTE: NOT IMPLEMENTED YET
# _AWS_V4_SIGNING = False
# _AWS_ACCESS_KEY = ""
# _AWS_SECRET_KEY = ""
# _REQUEST_TIMEOUT = 5
# _VERIFY_SSL = True
# _MAX_RETRIES = 1


def filter_content(content: bytes, headers: dict):
    global _FILTER_CONTENT
    if not _FILTER_CONTENT:
        return content
    return filter_module.filter_content(content, headers)


class ProxyRequestHandler(BaseHTTPRequestHandler):

    # incremented at each request, cannot be an instance, because a new
    # instance is created at each http request received
    count: int = 0
    log_function: T.Callable = logging.info

    # Private interface ###################################################

    def log_message(self, format: str = "", *args):
        s = format
        for a in args:
            s = s.replace('%s', a, 1)
        ProxyRequestHandler.log_function(s)

    def _log(self, msg):
        global _MUTE
        if _MUTE:
            return
        self.log_message(msg)

    def _print_text_header(self):
        headers_text = ""
        for (k, v) in self.headers.items():
            headers_text += f'{k}: {v}\n'
        return headers_text

    def _print_reqline_and_headers(self):
        ProxyRequestHandler.count += 1
        return "\n===> " + ">"*20 + '\n' + \
               f"**REQUEST #: {ProxyRequestHandler.count}\n" + \
               "------REQUEST LINE:\n" + \
               self.requestline + "\n\n" + \
               "------HEADERS" + "\n" + \
               self._print_text_header()

    def _print_response(self, resp):
        return "\n" + "<"*20 + ' <===\n' + \
               f"**RESPONSE #: {ProxyRequestHandler.count}\n" + \
               "------STATUS: " + str(resp.status_code) + '\n' + \
               "------HEADERS" + "\n" + \
               str(resp.headers) + "\n" + \
               "------CONTENT\n" + resp.text

    def _send_default_response(self, content=None, method=None):
        # method not currently used
        if not content:
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(("Request line: " + self.requestline + '\n' +
                              "\nRequest Headers:\n" + str(self.headers) + '\n')
                             .encode())
        else:
            self.send_response(200)
            content_length_sent = False
            for k, v in self.headers.items():
                if k.lower() == "content-length":
                    self.send_header(k, len(content))
                    content_length_sent = True
                else:
                    self.send_header(k, v)
            if not content_length_sent:
                self.send_header("Content-Length", len(content))
            self.end_headers()
            self.wfile.write(content)

    def _inject_auth(self, headers):
        return headers

    def _parse_headers(self):
        headers = self.headers
        headers['Host'] = self.host
        return self._inject_auth(headers)

    def _send_headers(self, headers):
        for (k, v) in headers.items():
            self.send_header(k, v)

    def _url(self):
        r = requests.head(self.remote_url + self.path, allow_redirects=True)
        return r.url

    def _send_response(self, resp):
        msg = "\n" + str(resp.headers) + "\n"  # self._print_response(resp)
        self._log("RESPONSE HEADERS\n" + 20*"=" + "\n" + msg)
        msg = ""
        self.send_response(resp.status_code)
        self._send_headers(resp.headers)
        self.end_headers()
        count = 0
        size = 0
        for i in resp.iter_content(chunk_size=self.chunk_size):
            self.wfile.write(i)
            count += 1
            size = max(size, len(i))
        msg += "\nnumber of chunks: " + str(count) + \
               "\nmax chunk length: " + str(size)
        self._log(msg)

        # encoding = None
        # for k in resp.headers.keys():
        #     if k.lower() == "transfer-encoding":
        #         encoding = resp.headers[k]
        # if encoding and encoding.lower().strip() == "chunked":
        #     count = 0
        #     size = 0
        #     for i in resp.iter_content(chunk_size=self.chunk_size):
        #         p = bytearray(f"{len(i):x}\r\n".encode('utf-8'))
        #         p.extend(i)
        #         p.extend("\r\n".encode())
        #         self.wfile.write(p)
        #         count += 1
        #         size = max(size, len(i))
        #     self.wfile.write(f"{0:x}\r\n\r\n".encode())
        #     self.wfile.write("\r\n".encode())
        #     msg += f"\nNumber of chunks: {count}, max chunk size: {size} "+ \
        #            "bytes\n\n"
        # else:
        #     msg += str(resp.headers)
        #     self.wfile.write(resp.content)
        #     msg += "\ncontent length: " + str(len(resp.content))

    def _handle_rest_request(self, json_content):
        config = json.loads(json_content)
        global _REMOTE_URL
        global _DOWNLOAD_CHUNK_SIZE
        result = "no recongnized parameter name in request"
        if "_REMOTE_URL" in config.keys():
            _REMOTE_URL = config["_REMOTE_URL"]
            result = "OK"
        if "_DOWNLOAD_CHUNK_SIZE" in config.keys():
            _DOWNLOAD_CHUNK_SIZE = config["_DOWNLOAD_CHUNK_SIZE"]
            result = "OK"
        if "get_config" in config.keys():
            result = f'{{"_REMOTE_URL": "{_REMOTE_URL}",' + \
                     f'"_DOWNLOAD_CHUNK_SIZE": "{_DOWNLOAD_CHUNK_SIZE}"' + \
                     "}}"
            return result

        return f'{{"result": "{result}"}}'

    def _read_content(self):
        r = re.compile("[Cc]ontent-[Ll]ength")
        cl = list(filter(r.match, self.headers.keys()))
        if len(cl) == 0:
            return None
        return self.rfile.read(int(self.headers[cl[0]]))

    # Public interface ###################################################

    def __init__(self, request, client_address, server):
        self.remote_url = _REMOTE_URL
        self.chunk_size = _DOWNLOAD_CHUNK_SIZE
        parsed_uri = urlparse(self.remote_url)
        self.host = parsed_uri.netloc
        super(ProxyRequestHandler, self).__init__(
            request, client_address, server)

    def do_GET(self):
        self._log(self._print_reqline_and_headers())
        if self.remote_url:
            req_headers = self._parse_headers()
            resp = requests.get(self._url(), headers=req_headers, stream=True)
            self._send_response(resp)
        else:
            self._send_default_response()

    def do_HEAD(self):
        self._log(self._print_reqline_and_headers())
        if self.remote_url:
            req_headers = self._parse_headers()
            resp = requests.head(self._url(), headers=req_headers, stream=True)
            self._send_response(resp)
        else:
            self._send_default_response()

    def do_POST(self):
        self._log(self._print_reqline_and_headers())
        from array import array
        content = self._read_content()
        global _MAX_LOG_CONTENT_LENGTH
        if len(content) < _MAX_LOG_CONTENT_LENGTH:
            log_msg = "------REQUEST_BODY" + "\n" + \
                      f"{str(content)}"
            self._log(log_msg)
        if self.remote_url:
            req_headers = self._parse_headers()
            resp = requests.post(self._url(),
                                 data=filter_content(content, self.headers),
                                 headers=req_headers,
                                 stream=True)
            self._send_response(resp)
        else:
            self._send_default_response(filter_content(content, self.headers))

    def do_PUT(self):
        self._log(self._print_reqline_and_headers())
        from array import array
        content = self._read_content()
        global _MAX_LOG_CONTENT_LENGTH
        if len(content) < _MAX_LOG_CONTENT_LENGTH:
            log_msg = "------REQUEST_BODY" + "\n" + \
                      f"{str(content)}"
            self._log(log_msg)
        if self.remote_url:
            req_headers = self._parse_headers()
            resp = requests.put(self._url(),
                                data=filter_content(content, self.headers),
                                headers=req_headers,
                                stream=True)
            self._send_response(resp)
        else:
            self._send_default_response(filter_content(content, self.headers))

    def do_DELETE(self):
        self._log(self._print_reqline_and_headers())
        if self.remote_url:
            req_headers = self._parse_headers()
            resp = requests.delete(
                self._url(), headers=req_headers, stream=True)
            self._send_response(resp)
        else:
            self._send_default_response()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='web proxy and request logger, works as both an ' +
                    'echo server for web requests and actual proxy with ' +
                    'the capability to load custom filters to filter and ' +
                    'modify content')
    parser.add_argument('-p', '--port', dest='port', type=int,
                        required=True, help='tcp port')
    parser.add_argument('-e', '--endpoint', dest='endpoint', type=str,
                        required=False, help='address to forward requests to')
    parser.add_argument('-f', '--content-filter', dest='filter', type=str,
                        required=False,
                        help="python module implementing " +
                             "filter_content(content: bytes, headers: dict)" +
                             "-> bytes function applied to content")
    args = parser.parse_args()
    if args.endpoint:
        _REMOTE_URL = args.endpoint
    if args.filter:
        exec(f"import {args.filter} as filter_module")
        _FILTER_CONTENT = True
    port = args.port
    httpd = HTTPServer(('', port), ProxyRequestHandler)
    print(f"Starting http server on port {port}" +
          (f", forwarding requests to {_REMOTE_URL}" if _REMOTE_URL else ""))
    logging.basicConfig(level=logging.INFO)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\rexiting...")  # without '\r' '^C' is shown
        sys.exit(0)
    except Exception as e:
        print("Error: " + e, file=sys.stderr)

# TODO: currently no way to add a control path
# if "application/json" in [v.lower() for v in self.headers.values()]:
#     ret = self._handle_rest_request(content)
#     self.send_response(code=200)
#     self.send_header("Content-Type", "application/json")
#     self.end_headers()
#     self.wfile.write(ret.encode('utf-8'))
#     return

# webdownloader

An asynchronous HTTP/HTTPS file downloader written in C for Linux.

The project is built around non-blocking sockets, `epoll` (edge-triggered mode), asynchronous DNS resolution, and OpenSSL. It is capable of downloading multiple files simultaneously while using a single event loop.

## Features

- Asynchronous event-driven architecture
- HTTP and HTTPS support
- TLS via OpenSSL
- Concurrent downloads
- Asynchronous DNS resolver thread
- HTTP redirects (301)
- Content-Length downloads
- Basic Transfer-Encoding: chunked support
- Automatic filename detection from URL
- Linux-only

## Requirements

- Linux
- GCC
- OpenSSL
- pthread

## Build

```bash
make
```

This produces the executable:

```text
webd
```

## Usage

Download a single file:

```bash
./webd http://speedtest.tele2.net/100MB.zip
```

Download multiple files concurrently:

```bash
./webd \
    http://speedtest.tele2.net/1MB.zip \
    http://speedtest.tele2.net/10MB.zip \
    http://speedtest.tele2.net/100MB.zip
```

Downloaded files are saved in the current working directory.

## Architecture

The downloader uses a state machine for every connection.

```
DNS
  ↓
CONNECT
  ↓
TLS HANDSHAKE (HTTPS only)
  ↓
SEND REQUEST
  ↓
READ RESPONSE
  ↓
DOWNLOAD FILE
```

Networking is completely non-blocking and driven by `epoll`.

## HTTP support

Currently implemented:

- HTTP/1.1
- GET
- HEAD
- Redirects (301)
- Content-Length
- Connection: close
- Basic Transfer-Encoding: chunked

## Project structure

```
connection.c    Connection state machine
downloader.c    File download logic
http.c          HTTP request/response handling
parser.c        HTTP header parser
tls.c           OpenSSL wrapper
dns.c           Asynchronous DNS worker
queue.c         Thread-safe queue
url.c           URL parser
utils.c         Utility functions
```

## Benchmark

A simple benchmark script is included to compare download performance.

Run a single large download:

```bash
python3 benchmark.py
```

Run multiple concurrent downloads:

```bash
python3 benchmark.py --many-files
```

The script will:

- build the project automatically (if necessary)
- create a temporary working directory
- copy the executable
- download test files from `speedtest.tele2.net`
- print total execution time
- print downloaded file sizes
- remove temporary files automatically

Useful options:

```text
--many-files         Download multiple files simultaneously
--not-delete-files   Keep downloaded files
--not-delete-dir     Keep the temporary benchmark directory
```

Example:

```bash
python3 benchmark.py --many-files --not-delete-files
```

Example output:

```
Total time: 12.48

Downloaded files:

1MB.zip: 1048576 bytes (1.00 MB)
10MB.zip: 10485760 bytes (10.00 MB)
100MB.zip: 104857600 bytes (100.00 MB)
```

## Limitations

The project is still under development.

Current limitations include:

- HTTP/2 is not supported
- HTTP/3 is not supported
- Chunked transfer decoding is basic
- Resume downloads are not implemented
- Compression (gzip/br/zstd) is not supported

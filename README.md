# HTTP Server
## About
This is a simple HTTP 1.1 server implemented in C. It serves static files and generates Mandelbrot videos on the fly. 
- Serves static files from the `httpdocs` directory
- Responds to browser requests with appropriate content types (e.g., text/html, image/jpeg, video/mp4)
- Generates Mandelbrot vidoes with random center points upon request by URLs containing "mandel"
- Mandelbrot videos are generated in a seperate project running in a local directory see [Mandelbrot Video Generator](https://github.com/zachkohl1/Mandelbrot-Video-Generator/upload/main)

## Requirements
- C compiler (e.g., GCC)
- Copy of Mandelbrot-Video-Generator (adjust path in http_server.c to match your local directory)
- ffmpeg for video generation

## Usage
- By default, the server listens on port 80, but can be configured to listen on any port with `./http_server -p <port number>`

1. The server listens for incoming connections on the specified port.
2. When a client connects and sends a request (e.g., a GET request for a file), the server parses the request to determine the requested URL.
3. The server checks if the requested file exists in the httpdocs directory.
4. If the file exists, the server reads the file contents and sends them back to the client in chunks for large files (e.g., videos).
5. If the requested URL contains "mandel", the server generates a Mandelbrot video with a random center point using the mandelbrot-zachkohl1 project and ffmpeg. The generated video is then sent back to the client.
6. If the requested file is not found (404 Not Found), the server sends a corresponding error message to the client.

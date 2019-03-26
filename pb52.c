#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned char buf[512];
static size_t buf_pos = 0;
static size_t buf_length = 0;

static void fatal_errno(const char* context) {
  if (errno == 0) {
    fprintf(stderr, "%s: errno was not set but expected it to be\n", context);
  } else {
    fprintf(stderr, "%s: %s\n", context, strerror(errno));
  }
  exit(1);
}

/*
 * Reads a single byte from STDIN, buffered. Returns -1 on EOF, dies on
 * failure. Writes all bytes read to STDOUT as they are buffered.
 */
static int read_stdin() {
  if (buf_pos == buf_length) {
    size_t total_written = 0;
    int r = read(STDIN_FILENO, buf, sizeof(buf));
    if (r == 0) return -1;
    if (r == -1) fatal_errno("read stdin");
    buf_length = r;
    buf_pos = 0;

    do {
      ssize_t written = write(STDOUT_FILENO, buf, buf_length);
      if (written == -1) fatal_errno("write to stdout");
      total_written += written;
    } while (total_written < buf_length);
  }

  return buf[buf_pos++];
}

static char prefix_buffer[5];
static int prefix_buffer_pos = 0;

int check_prefix_buffer() {
  const char *prefix = "\033]52;";

  const char *cursor = prefix;
  int offset = prefix_buffer_pos;
  do {
    if (prefix_buffer[offset++ % sizeof(prefix_buffer)] != *cursor++) return 0;
  } while (*cursor);

  return 1;
}

void send_to_pbcopy() {
  FILE* pbcopy = popen("base64 -D | pbcopy", "w");
  if (!pbcopy) fatal_errno("could not start pbcopy");

  while (1) {
    int b = read_stdin();
    if (b == -1) {
      fprintf(stderr, "unexpected end of stdin in middle of copy text\n");
      exit(2);
    }
    if (b == '\007') break;

    if (EOF == fputc(b, pbcopy)) {
      fprintf(stderr, "could not send character to pbcopy\n");
      exit(4);
    }
  }

  errno = 0;
  int pclose_res = pclose(pbcopy);
  if (!pclose_res) return;
  if (errno != 0) fatal_errno("could not close pbcopy stdin stream");
  fprintf(stderr, "pbcopy returned error: %d\n", pclose_res);
  exit(1);
}

int main(int argc, char** argv) {
  memset(prefix_buffer, 0, sizeof(prefix_buffer));

  if (argc != 1) {
    fprintf(stderr, "expect no arguments\n");
    return 1;
  }

  while (1) {
    int b = read_stdin();
    if (b == -1) break;

    prefix_buffer[prefix_buffer_pos++] = b;

    prefix_buffer_pos %= sizeof(prefix_buffer);
    if (!check_prefix_buffer()) continue;

    while (1) {
      int c = read_stdin();
      if (c == -1) {
        fprintf(stderr,
                "unexpected end of stream when reading paste buffer name\n");
        exit(3);
      }
      if (c == ';') break;
    }

    send_to_pbcopy();
  }
}

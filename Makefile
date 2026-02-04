CC = cc

CFLAGS = -Wall -Wextra -O2 \
	$(shell pkg-config --cflags wayland-client xkbcommon)

LDFLAGS = $(shell pkg-config --libs wayland-client xkbcommon)

SRC = main.c \
      wlr-layer-shell-unstable-v1-protocol.c \
      xdg-shell-protocol.c

all: vim-screenshot

vim-screenshot: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f vim-screenshot

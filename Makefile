CC        ?= cc
SRCDIR     = src
BUILDDIR   = build
INCLUDEDIR = include
DEPSDIR    = deps
RESDIR     = resources
GAMEDIR    = dist
BINARY     = $(BUILDDIR)/betterspades
TOOLKIT   ?= SDL

MAJOR   = 0
MINOR   = 1
PATCH   = 5
POSTFIX = -tiger

PACKURL = https://aos.party/bsresources.zip
RESPACK = $(GAMEDIR)/bsresources.zip

CDEPS   := $(shell find $(DEPSDIR) -type f -name '*.c')
ODEPS   := $(CDEPS:$(DEPSDIR)/%.c=$(BUILDDIR)/%.o)
CFILES  := $(shell find $(SRCDIR) -type f -name '*.c')
OFILES  := $(CFILES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

CFLAGS  = -std=gnu99
CFLAGS += -DBETTERSPADES_MAJOR=$(MAJOR)
CFLAGS += -DBETTERSPADES_MINOR=$(MINOR)
CFLAGS += -DBETTERSPADES_PATCH=$(PATCH)
CFLAGS += -DBETTERSPADES_VERSION=\"v$(MAJOR).$(MINOR).$(PATCH)$(POSTFIX)\"
CFLAGS += -DGIT_COMMIT_HASH=\"$(shell git rev-parse HEAD)\"
CFLAGS += -DUSE_SOUND

UNAME := $(shell uname -s)

LDFLAGS =

ifeq ($(TOOLKIT),SDL)
	CFLAGS += -DUSE_SDL

	ifeq ($(UNAME),Linux)
		LDFLAGS += -lSDL2
	endif

	ifeq ($(UNAME),Darwin)
		LDFLAGS += /usr/local/Cellar/sdl2/2.0.3/lib/libSDL2.a
	endif
endif

ifeq ($(TOOLKIT),GLFW)
	CFLAGS += -DUSE_GLFW
	LDFLAGS += -lglfw
endif

ifeq ($(TOOLKIT),GLUT)
	CFLAGS += -DUSE_GLUT

	ifeq ($(UNAME),Linux)
		LDFLAGS += -lglut
	endif

	ifeq ($(UNAME),Darwin)
		LDFLAGS += -framework GLUT
	endif
endif

ifeq ($(UNAME),Linux)
	LDFLAGS += -lm -lopenal -lGL -lGLU -pthread
endif

ifeq ($(UNAME),Darwin)
	LDFLAGS += -liconv -framework Carbon -framework OpenAL -framework CoreAudio -framework AudioUnit -framework IOKit -framework Cocoa -framework OpenGL
endif

all: $(BUILDDIR) $(BINARY)

$(BINARY): $(OFILES) $(ODEPS)
	$(CC) -o $(BINARY) $(OFILES) $(ODEPS) $(LDFLAGS)

$(OFILES): $(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p `dirname $@`
	$(CC) -Wall $(CFLAGS) -c $< -o $@ -I$(INCLUDEDIR)

$(ODEPS): $(BUILDDIR)/%.o: $(DEPSDIR)/%.c
	mkdir -p `dirname $@`
	$(CC) -Wall $(CFLAGS) -c $< -o $@ -I$(INCLUDEDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

.PHONY : game
game: $(BINARY) $(RESPACK)
	mkdir -p $(GAMEDIR)
	cp $(BINARY) $(GAMEDIR)
	cp -r $(RESDIR)/* $(GAMEDIR)
	unzip -o $(RESPACK) -d $(GAMEDIR) || true

$(RESPACK):
	curl -o $(RESPACK) $(PACKURL)

clean:
	rm -rf $(OFILES) $(ODEPS) $(BINARY)

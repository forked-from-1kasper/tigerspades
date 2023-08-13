CC        ?= cc
SRCDIR     = src
BUILDDIR   = build
INCLUDEDIR = include
DEPSDIR    = deps
RESDIR     = resources
GAMEDIR    = dist
BINARY     = $(BUILDDIR)/betterspades

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

CFLAGS  = -DBETTERSPADES_MAJOR=$(MAJOR)
CFLAGS += -DBETTERSPADES_MINOR=$(MINOR)
CFLAGS += -DBETTERSPADES_PATCH=$(PATCH)
CFLAGS += -DBETTERSPADES_VERSION=\"v$(MAJOR).$(MINOR).$(PATCH)$(POSTFIX)\"
CFLAGS += -DGIT_COMMIT_HASH=\"$(shell git rev-parse HEAD)\"
CFLAGS += -DUSE_SDL -DUSE_SOUND

all: $(BUILDDIR) $(GAMEDIR) $(RESPACK)
binary: $(BUILDDIR) $(BINARY)

$(BINARY): $(OFILES) $(ODEPS)
	$(CC) -o $(BINARY) $(OFILES) $(ODEPS) /usr/local/Cellar/sdl2/2.0.3/lib/libSDL2.a libenet.a libGLEW.a libcglm.a libdeflate.a -liconv -framework Carbon -framework OpenAL -framework CoreAudio -framework AudioUnit -framework IOKit -framework Cocoa -framework OpenGL

$(OFILES): $(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p `dirname $@`
	$(CC) -Wall $(CFLAGS) -c $< -o $@ -I$(INCLUDEDIR)

$(ODEPS): $(BUILDDIR)/%.o: $(DEPSDIR)/%.c
	mkdir -p `dirname $@`
	$(CC) -Wall $(CFLAGS) -c $< -o $@ -I$(INCLUDEDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(GAMEDIR): $(BINARY)
	mkdir -p $(GAMEDIR)
	cp $(BINARY) $(GAMEDIR)
	cp -r $(RESDIR)/* $(GAMEDIR)

$(RESPACK):
	curl -o $(RESPACK) $(PACKURL)
	unzip -o $(RESPACK) -d $(GAMEDIR) || true

clean:
	rm -rf $(OFILES) $(ODEPS) $(BINARY)

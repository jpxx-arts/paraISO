CC = gcc

CFLAGS = -Wall -g -std=c99 -Isrc

SRCDIR := src
OBJDIR := obj
BINDIR := bin

SOURCES := $(wildcard $(SRCDIR)/*.c)
LIB_SRC := $(filter %iso9660.c, $(SOURCES))
APP_SRC := $(filter-out %iso9660.c, $(SOURCES))

LIB_OBJS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(LIB_SRC))
APP_OBJS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(APP_SRC))

APPS := $(patsubst $(SRCDIR)/%_iso.c, $(BINDIR)/%_iso, $(APP_SRC))

.PHONY: all
all: $(APPS)

.PHONY: clean
clean:
	@echo "Limpando diretórios 'obj' e 'bin'..."
	@rm -rf $(OBJDIR) $(BINDIR)

$(BINDIR)/%_iso: $(OBJDIR)/%_iso.o $(LIB_OBJS) | $(BINDIR)
	@echo "Ligando (linking) para criar $@"
	@$(CC) $(CFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@echo "Compilando $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BINDIR):
	@mkdir -p $(BINDIR)

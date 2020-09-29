PG_CONFIG ?= "pg_config"

all: r53db.so

r53db.so: C.go *.c *.go *.h
	go build -o r53db.so -buildmode=c-shared

C.go: C.go.in
	PG_CONFIG_INCLUDEDIR_SERVER="`$(PG_CONFIG) --includedir-server`" \
	envsubst <C.go.in >C.go

clean:
	rm -f C.go r53db.so

test:
	tests/run-all-tests

install: r53db.so
	install r53db.so "`$(PG_CONFIG) --pkglibdir`"
	install r53db.control "`$(PG_CONFIG) --sharedir`/extension/"
	install r53db--*.sql "`$(PG_CONFIG) --sharedir`/extension/"


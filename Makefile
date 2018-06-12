AUTH=goginet
NAME=openmpi
TAG=${AUTH}/${NAME}
EDITOR=code

all: build up run

up:
	docker-compose up -d

down:
	docker-compose down

build:
	docker build -t $(TAG) .

run:
	cp -r data /tmp/data
	docker-compose exec master run
	echo "For see result open /tmp/data/C.csv or use make open_result"

connect:
	docker-compose exec master bash

open_result:
	$(EDITOR) /tmp/data/C.csv

reload: down all

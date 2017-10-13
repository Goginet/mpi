
# Kipod Infrastructure Platform
#
# Authors:
#   Georgy Shapchits <georgy.schapchits@synesis.ru>
#
# Copyright (C) 2016 Synesis LLC. All rights reserved.
AUTH=goginet
NAME=openmpi
TAG=${AUTH}/${NAME}

up:
	docker-compose up -d

down:
	docker-compose down

build:
	docker build -t $(TAG) .

connect:
	docker-compose exec master bash

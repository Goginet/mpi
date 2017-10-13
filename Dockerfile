FROM ubuntu:16.04

MAINTAINER Georgy Shapchits <gogi.soft.gm@gmail.com>

ENV USER mpi
ENV HOME=/home/${USER}
ENV SSHDIR ${HOME}/.ssh/
ENV SRC_FILE ${HOME}/src/main.c
ENV BIN_FILE ${HOME}/bin/a.out

ADD scripts scripts
ADD configs ${HOME}/configs
ADD src ${HOME}/src
ADD ssh ${SSHDIR}

RUN apt update -y && \
    apt install -y apt-utils vim iputils-ping openssh-server build-essential mpich && \
    mkdir /var/run/sshd && \
    echo 'root:${USER}' | chpasswd && \
    adduser --disabled-password --gecos "" ${USER} && \
    mkdir -p ${SSHDIR} && \
    mkdir -p $(dirname ${BIN_FILE}}) && \
    chmod -R 600 ${SSHDIR}* && \
    chown -R ${USER}:${USER} ${SSHDIR} && \
    mpicc ${SRC_FILE} -o ${BIN_FILE} && \
    chmod +x scripts/*

CMD ["/usr/sbin/sshd", "-D"]
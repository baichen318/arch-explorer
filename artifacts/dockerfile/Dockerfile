# Copyright (c) Alibaba Corporation.
# Licensed under the xxx License.

FROM ubuntu:18.04

LABEL maintainer="xuechao.wxc@alibaba-inc.com" 

# working directory
WORKDIR /root/workspace

# common packages
## base utilities
RUN apt-get -y update \ 
    && apt-get install -y build-essential automake \
    && apt-get install -y curl wget git vim \
	&& apt-get install -y protobuf-compiler libprotobuf-dev \
    && apt-get install -y python3.8-dev 
    # && apt-get clean \
    # && rm -rf /var/lib/apt/lists/*
## pdflatex
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt install -y texlive-latex-base texlive-fonts-recommended texlive-fonts-extra texlive-latex-extra

# conda
ENV CONDA_DIR /opt/conda
RUN wget --quiet https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda.sh && /bin/bash ~/miniconda.sh -b -p /opt/conda
ENV PATH=$CONDA_DIR/bin:$PATH

COPY environment.yml /root/environment.yml
RUN conda env create -f /root/environment.yml
RUN echo "source /opt/conda/etc/profile.d/conda.sh" >> ~/.bashrc
RUN echo "conda activate micro23" >> ~/.bashrc

# for gem5's python library
ENV PYTHON_CONFIG /usr/bin/python3.8-config

# arch-explorer
# RUN git clone http://gitlab.alibaba-inc.com/baichen.bai/arch-explorer.git
# COPY $HOME/arch-explorer /root/

# default commands
CMD ["bash"]

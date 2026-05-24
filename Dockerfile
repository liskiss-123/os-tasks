FROM ubuntu:20.04 

RUN apt-get update && apt-get install -y \
    g++ make python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

CMD ["make", "test"]
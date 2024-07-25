FROM gcc:latest
#RUN apt-get update
#RUN apt-get install -y gcc valgrind make g++
COPY ./docker/entrypoint.sh /
RUN chmod +x entrypoint.sh
#ENTRYPOINT ["/entrypoint.sh"]

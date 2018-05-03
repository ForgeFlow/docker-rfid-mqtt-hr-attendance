FROM alpine

RUN apk add --no-cache mosquitto mosquitto-clients \
&& rm -rf /var/cache/apk/*

RUN mkdir -p /mqtt/config /mqtt/data /mqtt/log
COPY config /mqtt/config
RUN chown -R mosquitto:mosquitto /mqtt \
    && mosquitto_passwd -U /mqtt/config/passwd 
VOLUME ["/mqtt/config", "/mqtt/data", "/mqtt/log"]

EXPOSE 1883

ADD docker-entrypoint.sh /usr/bin/

ENTRYPOINT ["docker-entrypoint.sh"]

CMD ["/usr/sbin/mosquitto", "-c", "/mqtt/config/mosquitto.conf"]

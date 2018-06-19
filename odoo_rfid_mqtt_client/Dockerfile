FROM python:2.7.14-alpine3.6

WORKDIR /usr/src/app

COPY requirements.txt ./

RUN apk add --no-cache --virtual .build-deps \
    gcc \
    libc-dev \
    unixodbc-dev \
    && pip install --no-cache-dir -r requirements.txt \
    && apk del .build-deps \
    && rm -rf /var/cache/apk/*

COPY RFID_mqtt_client.py ./

CMD [ "python", "-u", "./RFID_mqtt_client.py"]

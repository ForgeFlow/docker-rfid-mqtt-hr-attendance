FROM python:2.7.14-alpine3.6

WORKDIR /usr/src/app

RUN apk add --no-cache gcc libc-dev unixodbc-dev

COPY requirements.txt ./

RUN pip install --no-cache-dir -r requirements.txt \
    && rm -rf /var/cache/apk/*

COPY RFID_mqtt_client.py ./

CMD [ "python", "-u", "./RFID_mqtt_client.py"]

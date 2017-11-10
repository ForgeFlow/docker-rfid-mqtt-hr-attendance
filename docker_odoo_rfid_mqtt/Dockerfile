FROM python:2.7.14-alpine3.6

WORKDIR /usr/src/app

COPY requirements.txt ./

RUN pip install --no-cache-dir -r requirements.txt

COPY RFID_mqtt_client.py ./

CMD [ "python", "-u", "./RFID_mqtt_client.py"]

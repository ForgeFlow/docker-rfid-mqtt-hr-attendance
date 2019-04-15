# docker-rfid-mqtt-hr-attendance

## Introduction

Opinionated [docker](https://www.docker.com) application to manage
[MQTT](http://mqtt.org) in order to register attendances using
[Odoo](www.odoo.com).

Odoo modules dependency available soon on v10
[OCA/hr](https://github.com/OCA/hr).

## Install Docker CE and docker-compose

**Docker Engine Community Edition:**

```bash
> curl -fsSL get.docker.com -o get-docker.sh
> sh get-docker.sh
```

Once is installed you can add any user to docker group to enable access to
docker commands without using sudo:

```bash
> sudo usermod -aG docker $USER
```

After that, it is need to 'Log Out' session to enable changes.

**Docker-Compose:**

It is preferable install via pip

```bash
    pip install docker-compose
```

## Configuration

Supposing docker is already installed, let's proceed to configure containers
before run them up.

In ``mosquitto`` folder it is found a file called ``passwd``. There we must
place ``$USER:$PASSWORD`` allowed to join to mosquitto broker.

You must provide variables using ``.env`` file in order to configure mqqt
python client


**Attention!** Keep this text file clean from comments and final line spaces :)

```env
### ODOO ###
# Odoo URL
ODOO_HOST=localhost

# Odoo Port (if using proxy use 443 or 80)
ODOO_PORT=8069

# Odoo User name. Preferable specific user to manage attendances only.
ODOO_USER_NAME=admin
ODOO_PASSWORD=admin

# db_name for odoo production instance.
PGDATABASE=v11_hr_attendance_rfid


### MQTT ###
# mqtt URL, no need modifications for general usage.
MQTT_HOST=localhost
# Keep it this way unless you know what you do. I wouldn't
MQTT_PORT=1883

# MQTT credentials for internal client associated with Odoo
# Edit both look into /mosquitto/config/passwd $MQTT_USER:$MQTT_PASS
MQTT_USER=python_rfid
MQTT_PASS=1234
# HMAC KEY: Encrypt key shared by mqtt clients to codify messages. 16 ASCII char
KEY=M1k3y1sdAb3St0n4

# DEBUG Display many features on odoo_rfid_mqtt_client log, useful to configure it right.
DEBUG=True
```

## Run

After all configuration you are ready to run containers. Just:

```bash
docker-compose build --pull
docker-compose up -d
docker-compose logs -f
```

## Version

Version 1.1 compatible with [NodeMCU](https://github.com/Eficent/nodemcu_rfid_wifimanager) version 1.1
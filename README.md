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

In ``odoo_rfid_mqtt/secrets`` folder it is found next variables that should be
filled.

**Attention!** Keep this text file clean from comments and final line spaces :)

```txt
host:odoo.example.com               # Odoo URL
port:8069                           # Odoo Port (if using https use 443)
user_name:rfid_attendance           # Odoo User name. Preferable specific user to manage attendances only.
user_password:PaSsWoRd              # Odoo User password
dbname:example_db                   # db_name for odoo production instance.
mqtt_id:mosquitto                   # mqtt URL, no need modifications for general usage.
mqtt_port:1883                      # Default, see docker-compose.yml file.
mqtt_user:python_rfid               # Related to mosquitto passwd file.
mqtt_pass:1234                      # Related to mosquitto passwd file.
key:SuPeRhArDpAsSwOrD               # Encrypt key shared by mqtt clients to codify messages.
```

## Run

After all configuration you are ready to run containers. Just:

```bash
docker-compose build --pull
docker-compose up -d
docker-compose logs -f
```

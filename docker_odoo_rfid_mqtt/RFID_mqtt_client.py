import xmlrpclib
import socket
import paho.mqtt.client as mqtt

import os
import urlparse

user_id = os.environ.get('USERID')
user_password = os.environ.get('DB_PASSWORD')
object_facade = None
dbname = os.environ.get('PGDATABASE')

mqtt_id = os.environ.get('MQTTHOST')
mqtt_user = os.environ.get('MQTTUSER')
mqtt_pass = os.environ.get('MQTTPASS')

def connection(host, port, user, user_pw, database):
    global user_password
    user_password = user_pw
    global dbname
    dbname = database
    url_template = "http://%s:%s/xmlrpc/%s"
    print "URL: ", url_template % (host.replace("http://", ""), port, 'common')
    login_facade = xmlrpclib.ServerProxy(url_template % (host.replace("http://", ""), port, 'common'))
    global user_id
    user_id = login_facade.login(database, user, user_pw)
    print "USER: ", user_id
    global object_facade
    object_facade = xmlrpclib.ServerProxy(url_template % (host, port, 'object'))
    print "object_facade: ", object_facade


def create(model, data, context=None):
    if context is None:
        context = {}
    res = object_facade.execute(dbname, user_id, user_password,
                                model, 'create', data, context)
    return res


def search(model, query, offset=0, limit=False, order=False, context=None, count=False):
    if context is None:
        context = {}
    ids = object_facade.execute(dbname, user_id, user_password,
                                model, 'search', query, offset, limit, order, context, count)
    return ids


def read(model, ids, fields, context=None):
    if context is None:
        context = {}
    data = object_facade.execute(dbname, user_id, user_password,
                                 model, 'read', ids, fields, context)
    return data


def execute(model, method, *args, **kw):
    res = object_facade.execute(dbname, user_id, user_password,
                                model, method, *args, **kw)
    return res

def check_last_attendance_state(id):
    execute("hr.attendance", "check.last.attendance.state", id)

def consulta(num):
    retorno = {"employeeId": 0, "employeeName": ""}
    rfid_ids = search("hr.employee.rfid.key", [('cardId', '=', num)])
    if rfid_ids:
        rfid_data = read("hr.employee.rfid.key", rfid_ids[0], ["employee_id", "employee_name"])
        employee_id = rfid_data[0]['employee_id']
        employee_name = rfid_data[0]['employee_name']
        card_id = rfid_data[0]['id']
        # attendance_state = check_last_attendance_state(employee_id)
        create("hr.employee.rfid.key.log",
               {'description': "%s logged with card id %s" % (employee_name, num),
                'employee_id': employee_id})
        retorno["employeeId"] = employee_id
        retorno["employeeName"] = employee_name
    else:
        create("hr.employee.rfid.key.log", {'description': "Card not found. Id %s" % num})

    return retorno


def registro(employeeData):
    try:
        execute("hr.employee", "register_rfid_attendance_event", [employeeData["employeeId"]])
        return "FOUND"
    except Exception as e:
        return "FALSE"


# Overwrites MQTT events

# Is executed on new connection
def on_connect(mosq, obj, rc):
    print("rc: " + str(rc))


# Is executed on new message
def on_message(mosq, obj, msg):
    print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))
    host, port, user, user_pw, database, card_id = str(msg.payload).split("###")
    connection(host, port, user, user_pw, database)
    cons = consulta(card_id)

    if (cons["employeeId"] != ""):
        print(cons)
        retorno = registro(cons)
        print(retorno)
        mqttc.publish("retorno", retorno)
    else:
        mqttc.publish("retorno", "FALSE")


# Is executed to send messages
def on_publish(mosq, obj, mid):
    print("Publish: " + str(mid))


# Is executed on topic subscribe
def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed: " + str(mid) + " " + str(granted_qos))


# Is executes when writing log
def on_log(mosq, obj, level, string):
    print(string)


# New MQTT client
mqttc = mqtt.Client()

# Overwrites MQTT events
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_publish = on_publish
mqttc.on_subscribe = on_subscribe

# user and password cloudmqtt
mqttc.username_pw_set(mqtt_user, mqtt_pass)
# CLOUDMQTT url where the rows to read stay
mqttc.connect(mqtt_id, 1883)

# subscribe to topic
mqttc.subscribe("acesso", 0)

# loop searching for new data to read
rc = 0
while rc == 0:
    rc = mqttc.loop()
print("rc: " + str(rc))

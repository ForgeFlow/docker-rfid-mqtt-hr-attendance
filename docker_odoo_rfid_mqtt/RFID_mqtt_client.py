import xmlrpclib
import socket
import paho.mqtt.client as mqtt

import os
import urlparse

from Crypto.Cipher import AES
from Crypto.Hash import SHA256, HMAC
import binascii
import base64
import random, time

object_facade = None

d = {}

with open("./secrets/sensitiveInfo", "r") as sensInfo:

	#textfile = sensInfo.read().splitlines()
    for line in sensInfo:
        key,val = line.split(":")
        d[str(key)] = val[:-1]
        print("KEY: " + str(key) + "|" + "VAL: " + str(val))

user_id = d["user_id"]
host = d["host"]
port = d["port"]
user_name = d["user_name"]
user_password = d["user_password"]
dbname = d["dbname"]
mqtt_id = d["mqtt_id"]
mqtt_user = d["mqtt_user"]
mqtt_pass = d["mqtt_pass"]
#ivkey = d["ivkey"]
key = d["key"]
staticiv = d["staticiv"]

cnt = 0
r = 0
same = True


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

def compare_digest(x, y):
    if not (isinstance(x, bytes) and isinstance(y, bytes)):
        raise TypeError("both inputs should be instances of bytes")
    if len(x) != len(y):
        return False
    result = 0
    for a, b in zip(x, y):
        result |= ord(a) ^ ord(b)
    return result == 0


# Overwrites MQTT events

# Is executed on new connection
def on_connect(mosq, obj, rc):
    print("rc: " + str(rc))


# Is executed on new message
def on_message(mosq, obj, msg):

	global cnt

	global r, same, flag_auth

	global host, port, user_name, user_password, dbname

	if msg.topic == 'reset':

		cnt = 0;
		iv_t = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
		print("RESET!!!!!!!!!!!!!!!")
		

	elif msg.topic == 'acceso':

		if same == False or flag_auth == True:
			mqttc.publish("retorno", "NOAUTH")
			print("++++++++++++++HMAC authentication failed++++++++++++++")
		else:
			flag_auth = True
			same = False
			print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))

			card_id_b64 = msg.payload
			
			card_id_aux = base64.b64decode(card_id_b64)

			print("CARD (encryptd and coded): " + card_id_aux)
			decryption_suite = AES.new(key,AES.MODE_CBC,r)
			card_id_aux = decryption_suite.decrypt(card_id_aux)
			print("CARD (coded): " + card_id_aux)
			card_id_aux = base64.b64decode(card_id_aux)

			print("ID: " + card_id_aux)

			card_id = card_id_aux[-8:]

			print("#################################################################################")
			print("PARAMETERS: " + str(host) + " / " + str(port) + " / " + str(user_name) + " / " + str(user_password) + " / " + str(dbname))
			print("#################################################################################")
			connection(host, port, user_name, user_password, dbname)

			if check_id_integrity(card_id):

				cons = consulta(card_id)

				if (cons["employeeId"] != ""):
					print(cons)
					retorno = registro(cons)
					print(retorno)
					mqttc.publish("retorno", retorno)
					r = os.urandom(16)
					r = set_range(r)
				
				else:
					mqttc.publish("retorno", "FALSE")

			else:
				print("--------------- CARD ID INTEGRITY COMPROMISED ----------------")
				mqttc.publish("retorno", "NOAUTH")

		print("Trial: " + str(cnt))
		cnt = cnt + 1

	elif msg.topic == "hmac":
		print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))
		hmac = HMAC.new(key,r,SHA256)
		print key
		computed_hash_hex = hmac.hexdigest()
		print("HMAC(HEX): " + computed_hash_hex)

		hashb64 = base64.b64decode(msg.payload)
		hash_from_arduino = hashb64.encode("hex")
		print("HMAC_ESP8266(HEX): " + hash_from_arduino)

		same = compare_digest(computed_hash_hex,hash_from_arduino)
		flag_auth = False
		print("HMAC Comparison: " + str(same))

	else:
		r = os.urandom(16)
		r = set_range(r)
		mqttc.publish("ack", r)
		print("Session ID: " + r)


def set_range(l):
	for i in xrange(len(l)):
		s = l[i]
		while (s<'!' or s>'}'):
			s = os.urandom(1)
		if i == 0:
			p = s
		else:
			p = p + s
	return str(p)

def check_id_integrity(nid):
	checksum = 0
	for i in xrange(len(nid)):
		#print("ID[" + str(i) + "]: " + nid[i])
		if (nid[i]>='0' and nid[i]<='9') or (nid[i]>='a' and nid[i]<='z'):
			checksum = checksum + 1
		#print("Sum[" + str(i) + "]: " + str(checksum))
	if checksum == 8:
		return True
	else:
		return False


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
mqttc.subscribe("acceso", 0)
mqttc.subscribe("reset",0)

mqttc.subscribe("init",0)
mqttc.subscribe("hmac",0)

# loop searching for new data to read
rc = 0
while rc == 0:
    rc = mqttc.loop()
print("rc: " + str(rc))


"""
WSGI config for houseMonitoring project.

It exposes the WSGI callable as a module-level variable named ``application``.

For more information on this file, see
https://docs.djangoproject.com/en/4.0/howto/deployment/wsgi/
"""

import os

from django.core.wsgi import get_wsgi_application
from temperature.sensor_communication import SensorCommunication, Sensor

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'houseMonitoring.settings')

application = get_wsgi_application()

sensor = SensorCommunication()
sensor.start()

sensor2 = Sensor()
sensor2.start()
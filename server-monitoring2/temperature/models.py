from django.db.models import Model, DateTimeField, FloatField

class TemperatureSerre(Model):
    received_time = DateTimeField('date received')
    temperature_celsius = FloatField()

    def add():
        objects.create(received_time=datetime.datetime.now(), temperature_celsius=23.5)
from django.db import models

class TemperatureSerre(models.Model):
    received_time = models.DateTimeField('date received')
    temperature_celsius = models.FloatField()

    def kpi_temp(self):
        return self.received_time >= timezone.now() - datetime.timedelta(days=1)

    def graph_temp(self):
        return self.received_time >= timezone.now() - datetime.timedelta(days=1)

    def alert(self):
        return self.received_time >= timezone.now() - datetime.timedelta(days=1)
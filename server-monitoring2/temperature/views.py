from django.shortcuts import render
from django.http import HttpResponse
from django.db.models import Min, Max, Avg, DateTimeField
from django.db.models.functions import TruncHour
from temperature.models import TemperatureSerre
import json
import datetime
import logging


def home(request):
    return render(request, 'index.html')

def kpi_temp(request):
    response = {
        "currentTemp": TemperatureSerre.objects.filter(received_time__gte = datetime.datetime.now() - datetime.timedelta(minutes=30)).order_by("-received_time").values('temperature_celsius')[0]['temperature_celsius'], 
        "minTemp": TemperatureSerre.objects.filter(received_time__gte = datetime.datetime.now() - datetime.timedelta(days=1)).aggregate(Min('temperature_celsius'))["temperature_celsius__min"], 
        "maxTemp": TemperatureSerre.objects.filter(received_time__gte = datetime.datetime.now() - datetime.timedelta(days=1)).aggregate(Max('temperature_celsius'))["temperature_celsius__max"]
    }
    return  HttpResponse(json.dumps(response), content_type="application/json")

def graph_temp(request):
    temps = TemperatureSerre.objects.filter(
        received_time__gte = datetime.datetime.now() - datetime.timedelta(days=7)
        ).annotate(
            hour_slot=TruncHour("received_time")
        ).values("hour_slot").annotate(
            avg_temp=Avg('temperature_celsius')
        ).order_by("hour_slot")

    values = []

    for temp in temps:
        values.append({
            "time": temp['hour_slot'].strftime("%Y-%m-%d %H:%M:%S"), 
            "temp": temp['avg_temp']
        })

    response = {"temps": values}
    return HttpResponse(json.dumps(response), content_type="application/json")

def alert(request):
    response = {"status": "tempLow"}
    return  HttpResponse(json.dumps(response), content_type="application/json")

def buzzer(request):
    response = {"status": "enabled"}
    return  HttpResponse(json.dumps(response), content_type="application/json")
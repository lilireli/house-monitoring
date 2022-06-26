from django.shortcuts import render
from django.http import HttpResponse
import json


def home(request):
    return render(request, 'index.html')

def kpi_temp(request):
    response = {"currentTemp": 20.7, "minTemp": 1.2, "maxTemp": 2.3}
    return  HttpResponse(json.dumps(response), content_type="application/json")

def graph_temp(request):
    response = {
        "temps": [{"time": "2022", "temp": 1.4}]
    }
    return HttpResponse(json.dumps(response), content_type="application/json")

def alert(request):
    response = {"status": "ok"}
    return  HttpResponse(json.dumps(response), content_type="application/json")

def buzzer(request):
    response = {"status": "enabled"}
    return  HttpResponse(json.dumps(response), content_type="application/json")
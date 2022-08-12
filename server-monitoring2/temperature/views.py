from django.shortcuts import render
from django.http import HttpResponse
from django.db.models import Min, Max, Avg
from django.db.models.functions import TruncHour
from django.views.decorators.csrf import csrf_exempt
from temperature.models import TemperatureSerre
from houseMonitoring.settings import AUTH_KEY
import json
import datetime
import logging

CURRENT_STATUS = "ok"
ALARM_ENABLED = "on"

def home(request):
    return render(request, 'index.html')

def kpi_temp(request):
    global CURRENT_STATUS
    current_temp, min_temp, max_temp = (None, None, None)

    try:
        current_temp = TemperatureSerre.objects.filter(
            received_time__gte = datetime.datetime.now() - datetime.timedelta(minutes=30)
        ).order_by("-received_time").values('temperature_celsius')[0]['temperature_celsius']

    except Exception as err:
        logging.warning(f"Could not get current temp as {err}")
        CURRENT_STATUS = "errNoData"

    try:
        temps = TemperatureSerre.objects.filter(
            received_time__gte = datetime.datetime.now() - datetime.timedelta(days=1)
        ).aggregate(
            min_temp = Min('temperature_celsius'), 
            max_temp = Max('temperature_celsius')
        )
        
        min_temp = temps["min_temp"]
        max_temp = temps["max_temp"]

    except Exception as err:
        logging.warning(f"Could not get min and max temp as {err}")
        CURRENT_STATUS = "errNoData"

    response = {
        "currentTemp": current_temp, 
        "minTemp": min_temp, 
        "maxTemp": max_temp
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
    global CURRENT_STATUS
    response = {"status": CURRENT_STATUS}
    return  HttpResponse(json.dumps(response), content_type="application/json")

@csrf_exempt 
def buzzer(request):
    global ALARM_ENABLED

    if request.method == "GET":
        response = {"status": ALARM_ENABLED}
        return  HttpResponse(json.dumps(response), content_type="application/json")

    elif request.method == "POST":
        data = json.loads(request.body)
        ALARM_ENABLED = data['status']
        return  HttpResponse({'status': 'alarm set'}, content_type="application/json")

@csrf_exempt 
def add_temp(request):
    global CURRENT_STATUS
    global ALARM_ENABLED

    # delete data at noon
    curr_time = datetime.datetime.now()
    
    if curr_time.hour == 12 and curr_time.minute < 20:
        TemperatureSerre.objects.filter(
            received_time__lte = datetime.datetime.now() - datetime.timedelta(days=60)
        ).delete()

    if request.method == "POST":
        try:
            if request.POST['auth_key'] == AUTH_KEY:
                CURRENT_STATUS = request.POST['alarm_current']

                if CURRENT_STATUS != "errArduino":
                    temp = TemperatureSerre(
                        received_time=request.POST['datetime'], 
                        temperature_celsius=request.POST['temperature'])
                    temp.save()

                    logging.warning(f"Inserted new temperature {request.POST['temperature']} at {request.POST['datetime']}")

            else:
                logging.warning(f"Authentication key not recognised {err}")

        except Exception as err:
            logging.warning(f"Message badly formed as {err}")
            CURRENT_STATUS = "errRaspberry"
    
    response = {"alarm": ALARM_ENABLED}
    return HttpResponse(json.dumps(response), content_type="application/json")
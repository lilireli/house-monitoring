from django.urls import path

from . import views

urlpatterns = [
    path('', views.home, name='index'),
    path('kpi-temp', views.kpi_temp),
    path('graph-temp', views.graph_temp),
    path('alert', views.alert),
    path('buzzer', views.buzzer),
    path('temp/add', views.add_temp)
]
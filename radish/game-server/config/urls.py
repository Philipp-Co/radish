"""
URL-Konfiguration: nur die Instanz-API (siehe instances/urls.py).
"""
from django.urls import include, path

urlpatterns = [
    path('api/', include('instances.urls')),
]

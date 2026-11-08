"""
URL-Konfiguration: nur die Instanz-API (siehe instances/interfaces/urls.py).
"""
from django.urls import include, path

urlpatterns = [
    path('api/', include('instances.interfaces.urls')),
]

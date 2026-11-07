"""
URL configuration for config project.

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/5.2/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  path('', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  path('', Home.as_view(), name='home')
Including another URLconf
    1. Import the include() function: from django.urls import include, path
    2. Add a URL to urlpatterns:  path('blog/', include('blog.urls'))
"""
from django.conf import settings
from django.contrib import admin
from django.urls import include, path
from django.views.static import serve

urlpatterns = [
    path('admin/', admin.site.urls),
    path('api/', include('api.interfaces.urls')),
    # Die von collectstatic gesammelten Static-Files (Admin-CSS/-JS, siehe
    # STATIC_ROOT in settings.py). Mit DEBUG liefert sie die staticfiles-App
    # schon vorher aus, ohne DEBUG (Docker-Image) nur dieser Eintrag -- sonst
    # landete "/static/..." im Catch-all von web.urls und bekaeme index.html.
    # lstrip: Django normalisiert STATIC_URL zu "/static/", Routen beginnen
    # aber ohne "/".
    path(f'{settings.STATIC_URL.lstrip("/")}<path:path>', serve, {'document_root': settings.STATIC_ROOT}),
    # Zuletzt eingehaengt: web.urls enthaelt einen Catch-all
    # ("<path:filename>", siehe web/urls.py), der sonst admin/ und api/
    # verdecken wuerde, wenn er vorher stuende.
    path('', include('web.urls')),
]

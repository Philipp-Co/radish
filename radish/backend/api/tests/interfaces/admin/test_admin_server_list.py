from rest_framework import status
from rest_framework.test import APITestCase

from api.models import (
    GameServer,
)

from api.tests.helpers import (
    ADMIN_TOKEN,
    PLAYER_TOKEN,
    User,
)


class AdminServerListTests(APITestCase):
    """
    Deckt admin_views.AdminServerListView ab: nur mit der Realm-Rolle
    "Radish-Admin" erreichbar, sonst 401 (kein Token) bzw. 403 (Token ohne
    diese Rolle, z.B. ein normaler Player).
    """

    def setUp(self):
        self.admin_user = User.objects.create(username="radish-admin-user")

    def test_requires_authentication(self):
        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)

    def test_player_role_is_not_sufficient(self):
        self.client.force_authenticate(user=self.admin_user, token=PLAYER_TOKEN)

        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)

    def test_admin_role_can_list_servers(self):
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)
        GameServer.objects.create(
            name="server-b", address="10.0.0.2", port=7001, is_occupied=True
        )
        self.client.force_authenticate(user=self.admin_user, token=ADMIN_TOKEN)

        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        names = [entry["name"] for entry in response.data["servers"]]
        self.assertEqual(names, ["server-a", "server-b"])

from rest_framework import status
from rest_framework.test import APITestCase


class LoginCallbackTests(APITestCase):
    """auth_views.LoginCallbackView: der error-Parameter darf nicht als HTML zurueckkommen."""

    def test_error_parameter_is_not_reflected_as_html(self):
        payload = "<script>alert(1)</script>"

        response = self.client.get("/api/login/callback/", {"error": payload})

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertTrue(response["Content-Type"].startswith("text/plain"))
        self.assertEqual(response["X-Content-Type-Options"], "nosniff")

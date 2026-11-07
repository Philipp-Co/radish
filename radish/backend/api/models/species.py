from django.db import models


class Species(models.Model):
    """
    Eine Spezies. Jede Armee gehoert genau einer an, und alles, was in ihr
    steckt (Einheitentypen, Waffen, Ausruestung), muss von derselben sein.
    """

    name = models.CharField(max_length=255, unique=True)

    class Meta:
        ordering = ["name"]

    def __str__(self):
        return self.name

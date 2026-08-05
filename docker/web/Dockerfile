FROM nginx:1.27-alpine

COPY docker/web/default.conf /etc/nginx/conf.d/default.conf
COPY radish/web /usr/share/nginx/html

EXPOSE 80

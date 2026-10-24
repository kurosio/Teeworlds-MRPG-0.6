FROM php:8.2-apache

RUN apt-get update && apt-get install -y \
    default-mysql-client \
    zip \
    unzip \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# pdo_sqlite обязателен: настройки, гостевые аккаунты и запросы живут в
# data/editor-studio.sqlite (api/store.php). pdo_mysql — для игровой базы.
RUN docker-php-ext-install mysqli pdo pdo_mysql pdo_sqlite

RUN a2enmod rewrite headers

WORKDIR /var/www/html

COPY . /var/www/html/

COPY apache-config.conf /etc/apache2/sites-available/editor-studio.conf

RUN a2dissite 000-default.conf && \
    a2ensite editor-studio.conf

RUN mkdir -p \
    /var/www/html/scenarios \
    /var/www/html/data/db-dumps \
    && chown -R www-data:www-data /var/www/html \
    && chmod -R 755 /var/www/html \
    && chmod -R 775 /var/www/html/scenarios \
    && chmod -R 775 /var/www/html/data

EXPOSE 80

CMD ["apache2-foreground"]
FROM php:8.2-apache

RUN apt-get update && apt-get install -y \
    default-mysql-client \
    zip \
    unzip \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

RUN docker-php-ext-install mysqli pdo pdo_mysql

RUN a2enmod rewrite headers

WORKDIR /var/www/html

COPY . /var/www/html/

RUN mkdir -p \
    /var/www/html/scenarios \
    /var/www/html/data/db-dumps \
    && chown -R www-data:www-data /var/www/html \
    && chmod -R 755 /var/www/html \
    && chmod -R 775 /var/www/html/scenarios \
    && chmod -R 775 /var/www/html/data

EXPOSE 80

CMD ["apache2-foreground"]
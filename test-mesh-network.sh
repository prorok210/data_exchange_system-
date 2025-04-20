#!/bin/bash
set -e 

echo "Checking existing Docker networks..."
EXISTING_NETWORKS=$(docker network ls --format "{{.Name}}")

if echo "$EXISTING_NETWORKS" | grep -q "esp-mesh-network"; then
    echo "Network esp-mesh-network already exists, removing..."
    docker network rm esp-mesh-network || true
fi

# Остановить и удалить существующие контейнеры, если они есть
echo "Stopping and removing existing containers..."
docker-compose down || true

# Запустить контейнеры в фоновом режиме
echo "Building and starting containers..."
docker-compose up --build -d

# Подключиться к логам всех контейнеров
echo "Attaching to container logs..."
docker-compose logs -f


# Pesto docker

## Build the ci image

```bash
docker build -t pesto:latest -f docker/debian.dockerfile .
docker run -it --rm pesto:latest bash
```

## Push the ci image to gitlab registry

```bash
docker login registry.gitlab.inria.fr
docker build -t registry.gitlab.inria.fr/crossett/pesto/pesto-ci -f docker/ci.dockerfile .
docker push registry.gitlab.inria.fr/crossett/pesto/pesto-ci
```

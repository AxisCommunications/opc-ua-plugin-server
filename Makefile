.PHONY: %.eap dockerbuild clean dev-image-armv7hf dev-image-aarch64 \
		devel-armv7hf devel-aarch64
# docker build container targets
%.eap:
	docker build --progress=plain \
		--build-arg ARCH=$(basename $@) \
		-o type=local,dest=. "$(CURDIR)"

dockerbuild: armv7hf.eap aarch64.eap

# build a Docker image to be used when developing the 'armv7hf' ACAP
dev-image-armv7hf:
	docker build --progress=plain \
		-f devel-containers/Dockerfile.devimg \
		--build-arg HOST_UID=$(shell id -u ${USER}) \
		--build-arg HOST_GID=$(shell id -g ${USER}) \
		--build-arg HOST_LOGIN=$(shell id -u -n) \
		--build-arg HOST_GROUP=$(shell id -g -n) \
		--build-arg ARCH=armv7hf \
		--tag "my_armv7hf_devimg" .

# build a Docker image to be used when developing the 'aarch64' ACAP
dev-image-aarch64:
	docker build --progress=plain \
		-f devel-containers/Dockerfile.devimg \
		--build-arg HOST_UID=$(shell id -u ${USER}) \
		--build-arg HOST_GID=$(shell id -g ${USER}) \
		--build-arg HOST_LOGIN=$(shell id -u -n) \
		--build-arg HOST_GROUP=$(shell id -g -n) \
		--build-arg ARCH=aarch64 \
		--tag "my_aarch64_devimg" .

# start a development container for 'armv7hf'
devel-armv7hf:
	docker run \
		-v $(shell pwd):/opt/app \
		-u $(shell id -u):$(shell id -g) \
		--rm -i -t my_armv7hf_devimg

# start a development container for 'aarch64'
devel-aarch64:
	docker run \
		-v $(shell pwd):/opt/app \
		-u $(shell id -u):$(shell id -g) \
		--rm -i -t my_aarch64_devimg

clean:
	$(MAKE) -C app clean
	rm -rf *.eap *LICENSE.txt *.old

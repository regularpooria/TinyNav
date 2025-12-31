FROM espressif/idf:release-v6.0

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt install -y -q \
  cmake \
  git \
  hwdata \
  libglib2.0-0 \
  libnuma1 \
  libpixman-1-0 \
  linux-tools-virtual \
  udev \
  && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/local/bin/usbip usbip `ls /usr/lib/linux-tools/*/usbip | tail -n1` 20

# Install ESP-ADF 
ENV ADF_PATH /opt/esp/esp-adf
RUN git clone -b v2.6 --recursive https://github.com/espressif/esp-adf.git $ADF_PATH

# QEMU
ENV QEMU_REL=esp-develop-20220919
ENV QEMU_SHA256=f6565d3f0d1e463a63a7f81aec94cce62df662bd42fc7606de4b4418ed55f870
ENV QEMU_DIST=qemu-${QEMU_REL}.tar.bz2
ENV QEMU_URL=https://github.com/espressif/qemu/releases/download/${QEMU_REL}/${QEMU_DIST}

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN curl -L \
    --retry 5 \
    --retry-delay 5 \
    --retry-connrefused \
    -o ${QEMU_DIST} ${QEMU_URL} \
  && echo "${QEMU_SHA256} *${QEMU_DIST}" | sha256sum --check --strict - \
  && tar -xf ${QEMU_DIST} -C /opt \
  && rm ${QEMU_DIST}


ENV PATH=/opt/qemu/bin:${PATH}
WORKDIR /workspaces/TinyNav

RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash", "-c"]
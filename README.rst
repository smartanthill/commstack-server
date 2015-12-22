SmartAnthill Communication Stack server
=======================================

.. image:: https://travis-ci.org/smartanthill/commstack-server.svg?branch=develop
    :target: https://travis-ci.org/smartanthill/commstack-server
.. image:: https://ci.appveyor.com/api/projects/status/mavv82wt7p7e3f60?svg=true
    :target: https://ci.appveyor.com/project/ivankravets/commstack-server/history

`SmartAnthill <https://github.com/smartanthill/smartanthill2_0>`_
(Network Service) uses this server to communicate with the end embedded device
using `Simple IoT Protocol stack <https://github.com/smartanthill/smartanthill-simpleiot>`_.

Building
--------

.. code-block:: bash

	# change working directory
	cd /tmp

	# 1. Clone SmartAnthil Embedded Project (we need "hal_common" interface)
	git clone https://github.com/smartanthill/smartanthill2_0-embedded.git

	# 2. Add SmartAnthill Embedded "hal_common" to C Preprocessor includes list
	export PLATFORMIO_BUILD_FLAGS="-I/tmp/smartanthill2_0-embedded/firmware/src/hal_common"

	# 3. Clone CommStack Server Project
	git clone https://github.com/smartanthill/smartanthill-commstack-server.git
	cd smartanthill-commstack-server
	git submodule update --init --recursive

	# 4. Build binaries using PlatformIO cross compiler
	cd server
	platformio run

	# 5. Binaries are ready in ".pioenvs/%SYSTEM_TYPE%/program"

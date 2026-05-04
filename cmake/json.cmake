#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#


# nlohmann_json
message(STATUS "Fetching nlohmann_json")

fetchcontent_declare(
    json #
    DOWNLOAD_EXTRACT_TIMESTAMP true #
    EXCLUDE_FROM_ALL
    URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz"
)
fetchcontent_makeavailable(json)

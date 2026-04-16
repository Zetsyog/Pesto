#!/usr/bin/env bash

set -euo pipefail

DOCKERFILE="docker/${1:-debian.dockerfile}"

# Configuration
REGISTRY="${CI_REGISTRY_IMAGE}"
# Default to empty if not set, script will fail gracefully if not provided

# Extract GitLab Environment Variables

BRANCH="${CI_COMMIT_BRANCH:-anonymous}"
TAG="${CI_COMMIT_TAG:-}"
SHORT_SHA="${CI_COMMIT_SHORT_SHA:-}"
SOURCE="${CI_PIPELINE_SOURCE:-}"

# Helper function to log
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1"
}

# 1. Determine the Primary Tag
PRIMARY_TAG=""
PUSH_LATEST=false
LATEST_TAG=""

if [ -n "$TAG" ]; then
    # Case: Tag Push (e.g., v1.0.0)
    # Logic: branch-tag (e.g., main-v1.0.0)
    PRIMARY_TAG="${REGISTRY}/${BRANCH}-${TAG}"
elif [ "$SOURCE" = "merge_request_event" ]; then
    # Case: Merge Request
    # Logic: branch-sha-mr (e.g., feature-xyz-abc123-mr)
    # We explicitly avoid pushing 'latest' here.
    PRIMARY_TAG="${REGISTRY}/mr:${BRANCH}-${SHORT_SHA}"
    log "Detected MR event. Tagging as '${PRIMARY_TAG}' (no latest)."

else
    # Case: Branch Push (no tag, not MR)
    # Logic: branch-sha (e.g., feature-xyz-abc123)
    PRIMARY_TAG="${REGISTRY}:${BRANCH}-${SHORT_SHA}"
    log "Detected branch push. Tagging as '${PRIMARY_TAG}'."
    # Check if we should push 'latest'
    # Requirement: If branch is main AND it's a tag push
    if [ "$BRANCH" = "main" ]; then
        LATEST_TAG="${REGISTRY}:latest"
        PUSH_LATEST=true
        log "Detected tag push on main branch. Will push 'latest'."
    else
        log "Detected tag push on non-main branch. No 'latest' tag."
    fi
fi

# Validate that we have a tag to push
if [ -z "$PRIMARY_TAG" ]; then
    log "ERROR: Could not determine a valid primary tag. Exiting."
    exit 1
fi

log "Building and pushing image: ${PRIMARY_TAG}"

# 2. Build the Image
# Assuming you have a Dockerfile in the root.
# If you use Nix to build the image first, adjust this command.
docker build -f "${DOCKERFILE}" -t "${PRIMARY_TAG}" .

# 3. Push the Primary Tag
docker push "${PRIMARY_TAG}"
log "Successfully pushed ${PRIMARY_TAG}"

# 4. Push 'latest' if conditions met
if [ "$PUSH_LATEST" = true ]; then
    log "Tagging and pushing 'latest' to ${LATEST_TAG}"
    docker tag "${PRIMARY_TAG}" "${LATEST_TAG}"
    docker push "${LATEST_TAG}"
    log "Successfully pushed ${LATEST_TAG}"
fi

echo "${PRIMARY_TAG}" >".gitlab-ci-docker-image"

log "Docker push workflow completed successfully."

#!/bin/bash


URL="http://127.0.0.1:9998/jsonrpc"
AUTH_HEADER="Authorization: Bearer"
LOG_FILE="/opt/logs/maintenance.log"

# Ensure log file exists (do NOT wipe it)
> /opt/logs/maintenance.log

EXPECTED_MODE="BACKGROUND"
EXPECTED_OPTOUT="IGNORE_UPDATE"
EXPECTED_TRIGGER="critical-update"

# ---------------------------------------------------------
# 1. Set Maintenance Mode to BACKGROUND with triggerMode
# ---------------------------------------------------------
echo "STEP 1: Setting maintenance mode to $EXPECTED_MODE with triggerMode=$EXPECTED_TRIGGER..."

curl -H "$AUTH_HEADER" \
     -H "Content-Type: application/json" \
     --request POST --silent \
     -d "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"org.rdk.MaintenanceManager.1.setMaintenanceMode\",\"params\":{\"maintenanceMode\":\"$EXPECTED_MODE\",\"triggerMode\":\"$EXPECTED_TRIGGER\",\"optOut\":\"$EXPECTED_OPTOUT\"}}" \
     "$URL"

echo -e "\n"

# ---------------------------------------------------------
# 2. Get & Verify Maintenance Mode
# ---------------------------------------------------------
echo "STEP 2: Getting maintenance mode (verification)..."

GET_RESPONSE=$(curl -H "$AUTH_HEADER" \
     -H "Content-Type: application/json" \
     --request POST --silent \
     -d '{"jsonrpc":"2.0","id":5,"method":"org.rdk.MaintenanceManager.1.getMaintenanceMode","params":{}}' \
     "$URL")

echo "Response: $GET_RESPONSE"
echo -e "\n"

# ---------------------------------------------------------
# 3. Trigger Maintenance Mode
# ---------------------------------------------------------
echo "STEP 3: Triggering startMaintenance..."

curl -H "$AUTH_HEADER" \
     -H "Content-Type: application/json" \
     --request POST --silent \
     -d '{"jsonrpc":"2.0","id":5,"method":"org.rdk.MaintenanceManager.1.startMaintenance","params":{}}' \
     "$URL"

echo -e "\n"

# ---------------------------------------------------------
# 4. Verify Maintenance Mode, triggerMode, and optOut in Log
# ---------------------------------------------------------
echo "STEP 4: Checking maintenance.log for maintenance entries..."

if [[ -f "$LOG_FILE" ]]; then
    sleep 5  # wait for the log to be updated

    LOG_MODE=$(grep -o '"maintenanceMode":"[^"]*"' "$LOG_FILE" | tail -1 | cut -d':' -f2 | tr -d '"')
    LOG_OPTOUT=$(grep -o '"optOut":"[^"]*"' "$LOG_FILE" | tail -1 | cut -d':' -f2 | tr -d '"')
    LOG_TRIGGER=$(grep -o '"triggerMode":"[^"]*"' "$LOG_FILE" | tail -1 | cut -d':' -f2 | tr -d '"')

    echo "Log maintenanceMode: $LOG_MODE"
    echo "Log optOut: $LOG_OPTOUT"
    echo "Log triggerMode: $LOG_TRIGGER"

    if [[ "$LOG_MODE" == "$EXPECTED_MODE" ]] && \
       [[ "$LOG_OPTOUT" == "$EXPECTED_OPTOUT" ]] && \
       [[ "$LOG_TRIGGER" == "$EXPECTED_TRIGGER" ]]; then
        echo "RESULT: PASS"
    else
        echo "RESULT: FAIL"
    fi
else
    echo "ERROR: Log file not found at $LOG_FILE"
fi

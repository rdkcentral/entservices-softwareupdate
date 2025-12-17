#!/bin/bash


URL="http://127.0.0.1:9998/jsonrpc"
AUTH_HEADER="Authorization: Bearer"
LOG_FILE="/opt/logs/maintenance.log"

#Clear log file
> /opt/logs/maintenance.log

EXPECTED_MODE="BACKGROUND"
EXPECTED_OPTOUT="IGNORE_UPDATE"

# ---------------------------------------------------------
# 1. Set Maintenance Mode to BACKGROUND
# ---------------------------------------------------------
echo "STEP 1: Setting maintenance mode to BACKGROUND..."

curl -H "$AUTH_HEADER" \
     -H "Content-Type: application/json" \
     --request POST --silent \
     -d "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"org.rdk.MaintenanceManager.1.setMaintenanceMode\",\"params\":{\"maintenanceMode\":\"$EXPECTED_MODE\",\"optOut\":\"$EXPECTED_OPTOUT\"}}" \
     "$URL"

echo -e "\n"

# ---------------------------------------------------------
# 2. Get & Verify Maintenance Mode
# ---------------------------------------------------------
echo "STEP 2: Getting maintenance mode..."

GET_RESPONSE=$(curl -H "$AUTH_HEADER" \
     -H "Content-Type: application/json" \
     --request POST --silent \
     -d '{"jsonrpc":"2.0","id":"3","method":"org.rdk.MaintenanceManager.1.getMaintenanceMode","params":{}}' \
     "$URL")

echo "Response: $GET_RESPONSE"
echo -e "\n"

# ---------------------------------------------------------
# 2A Verify GET_RESPONSE matches EXPECTED_MODE and EXPECTED_OPTOUT
# ---------------------------------------------------------

# Extract values from JSON response
REPORTED_MODE=$(echo "$GET_RESPONSE" | grep -o '"maintenanceMode":"[^"]*"' | cut -d':' -f2 | tr -d '"')
REPORTED_OPTOUT=$(echo "$GET_RESPONSE" | grep -o '"optOut":"[^"]*"' | cut -d':' -f2 | tr -d '"')

echo "Extracted maintenanceMode: $REPORTED_MODE"
echo "Extracted optOut: $REPORTED_OPTOUT"

# Compare both values
if [[ "$REPORTED_MODE" == "$EXPECTED_MODE" ]] && [[ "$REPORTED_OPTOUT" == "$EXPECTED_OPTOUT" ]]; then
    echo "Verification: PASS — maintenanceMode and optOut match expected values."
else
    echo "Verification: FAIL"
    echo "Expected maintenanceMode: $EXPECTED_MODE, Got: $REPORTED_MODE"
    echo "Expected optOut: $EXPECTED_OPTOUT, Got: $REPORTED_OPTOUT"
fi

echo -e "\n"

# ---------------------------------------------------------
# 3. Trigger Maintenance Mode
# ---------------------------------------------------------
echo "STEP 3: Triggering startMaintenance..."

curl -H "$AUTH_HEADER" \
     -H "Content-Type: application/json" \
     --request POST --silent \
     -d '{"jsonrpc":"2.0","id":"3","method":"org.rdk.MaintenanceManager.1.startMaintenance","params":{}}' \
     "$URL"

echo -e "\n"

# ---------------------------------------------------------
# 4. Verify Maintenance Mode in Log
# ---------------------------------------------------------
echo "STEP 4: Checking maintenance.log for maintenance entries"

if [[ -f "$LOG_FILE" ]]; then
    sleep 5

    LOG_MODE=$(grep -o '"maintenanceMode":"[^"]*"' "$LOG_FILE" | tail -1 | cut -d':' -f2 | tr -d '"')
    LOG_OPTOUT=$(grep -o '"optOut":"[^"]*"' "$LOG_FILE" | tail -1 | cut -d':' -f2 | tr -d '"')

    echo "Log maintenanceMode: $LOG_MODE"
    echo "Log optOut: $LOG_OPTOUT"

    if [[ "$LOG_MODE" == "$EXPECTED_MODE" ]] && [[ "$LOG_OPTOUT" == "$EXPECTED_OPTOUT" ]]; then
        echo "RESULT: PASS"
    else
        echo "RESULT: FAIL"
    fi
else
    echo "ERROR: Log file not found at $LOG_FILE"
fi

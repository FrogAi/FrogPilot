MIN_VISIBLE_ALPHA = 0.01

CE_STATUS = {
  "OFF": 0,              # Off
  "USER_DISABLED": 1,    # "Experimental Mode" disabled by user
  "USER_OVERRIDDEN": 2,  # "Experimental Mode" enabled by user
  "CURVATURE": 3,        # Road curvature condition
  "LEAD": 4,             # Slower lead vehicle condition
  "SIGNAL": 5,           # Turn signal condition
  "SPEED": 6,            # Speed condition
  "SPEED_LIMIT": 7,      # Speed limit controller condition
  "STOP_LIGHT": 8,       # Stop light or sign condition
}

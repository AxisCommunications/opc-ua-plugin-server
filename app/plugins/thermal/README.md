# Thermal Plugin

## Description

This plugin exposes thermal camera functionality as OPC-UA nodes, including:

- Thermal area configurations and status
- Temperature measurements (min, max, avg)
- Threshold settings and triggered states
- Temperature scale control (Celsius/Fahrenheit)

## Features

- **Thermal Area Management**: Exposes all configured thermal areas as OPC-UA
objects
- **Temperature Monitoring**: Provides real-time temperature readings
- **Threshold Control**: Shows threshold values and triggered states
- **Scale Conversion**: Allows changing temperature scale between Celsius and
Fahrenheit
- **Automatic Updates**: Regularly polls for updated temperature values

## Usage

The plugin creates the following OPC-UA structure:

- **ThermalAreas** object (parent node)
    - **Set Scale Method**: Method to change temperature scale (to Celsius or
    Fahrenheit)
    - Multiple **ThermalX** objects (one per configured thermal area)
        - Properties:
            - `Id`: Area identifier
            - `Name`: Area name
            - `Enabled`: Activation state (boolean)
            - `PresetNumber`: Associated preset number (integer)
            - `TempMin`: Minimum temperature reading (integer)
            - `TempMax`: Maximum temperature reading (integer)
            - `TempAvg`: Average temperature reading (integer)
            - `ThresholdValue`: Configured threshold value (integer)
            - `ThresholdMeasurement`: Measurement type ('above', 'below',
            'increasing', 'decreasing')
            - `Triggered`: Trigger state (boolean, true if threshold is met)
            - `DetectionType`: Detection type ('warmest', 'coldest', 'average')

## Important Notes

- This plugin requires an Axis thermal camera to be able to function.
- Thermal areas must be configured prior to starting the ACAP application.
- Any changes to thermal area configurations on the camera require a restart of
the OPC-UA Server ACAP application for them to be reflected.

## License

**[MIT License](../../../LICENSE)**

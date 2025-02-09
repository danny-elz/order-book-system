# OrderBook Project

## Overview

The OrderBook project is a C++ implementation of an order book system that manages and matches buy and sell orders. It supports different order types and handles trades between matching orders.

## Features

- **Order Management**: Add, cancel, and modify orders.
- **Order Matching**: Match buy and sell orders based on price and quantity.
- **Trade Handling**: Generate trades when orders are matched.
- **Order Levels**: Aggregate and retrieve order levels for bids and asks.

## Requirements

- CMake 3.29 or higher
- C++20 compatible compiler

## Building the Project

1. Clone the repository:
    ```sh
    git clone <repository_url>
    cd OrderBook
    ```

2. Create a build directory and navigate to it:
    ```sh
    mkdir build
    cd build
    ```

3. Run CMake to configure the project:
    ```sh
    cmake ..
    ```

4. Build the project:
    ```sh
    cmake --build .
    ```

## Usage

After building the project, you can run the executable:

```sh
./OrderBook
```

## Code Structure

- `main.cpp`: Contains the main function and the implementation of the OrderBook system.

## Classes

- **Order**: Represents an individual order with attributes like order type, ID, side, price, and quantity.
- **OrderModify**: Represents a modification request for an existing order.
- **Trade**: Represents a trade between a bid and an ask.
- **OrderBook**: Manages and matches orders, generates trades, and provides order level information.
- **OrderbookLevelInfos**: Aggregates order levels for bids and asks.




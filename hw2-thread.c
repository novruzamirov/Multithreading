/*
Name: Novruz Amirov
Student ID: 150200903
BLG 312E - Operating Systems - Assignment 2 - Multithreading
*/

// standard libraries
#include <stdio.h>
#include <stdlib.h>

// library to use threads
#include <pthread.h>

// library to use rand()
#include <time.h>

// You can change the number of customers and products and scenario (either 1 or 2) from HERE
#define NUM_PRODUCTS 6
#define NUM_CUSTOMERS 4
#define SCENARIO 2

// Product Struct
typedef struct
{
    int product_id;       // Unique value for identifying each product starts from 0 and incremented by 1.
    double product_price; // Price of the product (between 1 – 200$).
    int product_quantity; // Quantity of the products in stock (1-10 for each).
} Product;

// Customer Struct
typedef struct
{
    int customer_id;              // unique, starts from 1 increment by one.
    double customer_balance;      // between 0-200$
    double initial_balance;       // initial balance of the customer before buying anything
    int **ordered_items;          // The list of products to be ordered.
    int ordered_items_size;       // the number of the items ordered
    int ordered_items_max_size;   // max number used for making array works as vector, if size == max_size then add double free size to array
    int **purchased_items;        // The list of products to be purchased.
    int purchased_items_size;     // the number of items purchased
    int purchased_items_max_size; // making an array to behave like vector same as ordered max size
} Customer;

// to store the products in stock
typedef struct
{
    Product **products; // array which holds all products
    int size;           // the number of products in stock
} ProductList;

// to store the customers as a list
typedef struct
{
    Customer **customers; // Customer array with the size defined above NUM_CUSTOMERS
    int size;
} CustomerList;

// 3 Mutex Locks: 1) For printing on console 2) For alloc() or realloc() in mem 3) Any changes in products
pthread_mutex_t console_lock;        // lock for printing to the console
pthread_mutex_t mem_allocation_lock; // when reallocating and mallocating
pthread_mutex_t list[NUM_PRODUCTS];  // lock for each product, we do not need to lock the store for the 1 product trying to be bought

ProductList product_list;   // to store the list of product in stock
CustomerList customer_list; // to store the customers as a list to check their balance an etc.

// Function to generate a random float between min and max
double random_double(double min, double max)
{
    if (min == max)
    {
        printf("Borders of random_double() can not be same\n");
        exit(-1);
    }
    double range = max - min;
    double num = (double)rand() / RAND_MAX;
    return (num * range) + min;
}

// Function to generate a random integer between min and max
int random_int(int min, int max)
{
    int range = max - min;
    if (min == max)
    {
        printf("Borders of random_int() can not be same\n");
        exit(-1);
    }
    return (rand() % range) + min;
}

// to print the products in stock on console
void printProductList(ProductList *productList)
{
    for (int i = 0; i < productList->size; i++)
    {
        printf("(ID: %d) - (Quantity: %d) - (Price:$%.2f)\n", productList->products[i]->product_id,
               productList->products[i]->product_quantity, productList->products[i]->product_price);
    }
    printf("\n");
}

// to print the customers in customer list
void printCustomerList(CustomerList *customerList)
{
    printf("\nCustomer List:\n");
    for (int i = 0; i < customerList->size; i++)
    {
        printf("Customer%d------\n", customerList->customers[i]->customer_id);
        printf("Initial Balance: $%.2f\n", customerList->customers[i]->initial_balance);
        printf("Updated Balance: $%.2f\n", customerList->customers[i]->customer_balance);
        printf("Ordered Products: \n");
        printf("ID      Quantity\n");

        for (int j = 0; j < customerList->customers[i]->ordered_items_size; j++)
            printf("%d          %d\n", customerList->customers[i]->ordered_items[j][0], customerList->customers[i]->ordered_items[j][1]);

        printf("Purchased Products: \n");
        printf("ID      Quantity\n");
        for (int k = 0; k < customerList->customers[i]->purchased_items_size; k++)
            printf("%d          %d\n", customerList->customers[i]->purchased_items[k][0], customerList->customers[i]->purchased_items[k][1]);

        printf("\n");
    }
}

// Function to add a new item to the end of the ordered_items array
void add_product_ordered(Customer *customer, int id, int price)
{
    // Allocate memory for a new row in the ordered_items array
    pthread_mutex_lock(&mem_allocation_lock);

    if (customer->ordered_items_size == 0) // if it is the first time, then malloc
        customer->ordered_items = malloc(sizeof(int *) * (customer->ordered_items_max_size));

    if (customer->ordered_items_size == customer->ordered_items_max_size) // if only the size increases, then realloc
    {
        customer->ordered_items = realloc(customer->ordered_items, sizeof(int *) * (customer->ordered_items_max_size * 2));
        customer->ordered_items_max_size *= 2;
    }

    pthread_mutex_unlock(&mem_allocation_lock);
    // mutex can be unlocked not to waste time because each threads points to different customer, so no way to interrupt each other

    customer->ordered_items[customer->ordered_items_size] = malloc(sizeof(int) * 2); // to create a space for a product added to array
    customer->ordered_items[customer->ordered_items_size][0] = id;
    customer->ordered_items[customer->ordered_items_size][1] = price;
    customer->ordered_items_size++;
}

void add_product_purchased(Customer *customer, int product_id, int product_quantity)
{
    // Allocate memory for a new row in the ordered_items array
    pthread_mutex_lock(&mem_allocation_lock);
    if (customer->purchased_items_size == 0) // malloc if it is the first time
        customer->purchased_items = malloc(sizeof(int *) * (customer->purchased_items_max_size));

    if (customer->purchased_items_size == customer->purchased_items_max_size) // realloc if creating a new free space for array
    {
        customer->purchased_items = realloc(customer->purchased_items, sizeof(int *) * (customer->purchased_items_max_size * 2));
        customer->purchased_items_max_size *= 2;
    }
    pthread_mutex_unlock(&mem_allocation_lock);
    // mutex can be unlocked not to waste time because each threads points to different customer, so no way to interrupt each other

    // Allocate memory for the ID and price values and add them to the new row
    customer->purchased_items[customer->purchased_items_size] = malloc(sizeof(int) * 2);
    customer->purchased_items[customer->purchased_items_size][0] = product_id;
    customer->purchased_items[customer->purchased_items_size][1] = product_quantity;
    customer->purchased_items_size++;
}

// if the customer wants to order more than one product, SCENARIO 2
void purchase_products(Customer *customer, int *products, int *quantities, int number_of_products)
{
    int bought_products[number_of_products];   // to store which products customer purchased
    int bought_quantities[number_of_products]; // to store the number of each products purchased
    int bought = 0;                            // number of successful purchased products, initially 0

    for (int i = 0; i < number_of_products; i++)
    {
        if (product_list.products[products[i]]->product_quantity >= quantities[i]) // to check the number of products left in stock
        {
            // to check if the customer balance is good enough to buy the product with the wished quantities
            if (customer->customer_balance >= (product_list.products[products[i]]->product_price * quantities[i]))
            { // Successfully PURCHASED an item with suggested quantity
                if (bought == 0)
                { // just for output configuration on console
                    pthread_mutex_lock(&console_lock);
                    printf("\n");
                    printf("Customer%d:\n", customer->customer_id);
                    printf("Initial Products:\n");
                    printProductList(&product_list);
                    pthread_mutex_unlock(&console_lock);
                }

                product_list.products[products[i]]->product_quantity -= quantities[i];                             // decrease the number of items in stock
                customer->customer_balance -= (quantities[i] * product_list.products[products[i]]->product_price); // decrease the balance of the customer
                add_product_purchased(customer, products[i], quantities[i]);                                       // add the product to the customer's purchased products list
                bought_products[bought] = products[i];                                                             // add product to bought array
                bought_quantities[bought] = quantities[i];                                                         // add quantity to quantities array
                bought++;
                pthread_mutex_lock(&console_lock);
                printf("Customer%d(%d,%d) success! Paid %.2f for each.\n", customer->customer_id, products[i], quantities[i], product_list.products[products[i]]->product_price);
                pthread_mutex_unlock(&console_lock);
            }
            else
            { // if the customer does not have enough money to buy the products with number of quantity suggested
                pthread_mutex_lock(&console_lock);
                printf("Customer%d(%d,%d) fail! Insufficient funds.\n", customer->customer_id, products[i], quantities[i]);
                pthread_mutex_unlock(&console_lock);
            }
        }
        else
        { // if the suggested number of products do not exist in stock
            pthread_mutex_lock(&console_lock);
            printf("Customer%d(%d,%d) fail! Only %d left in stock\n", customer->customer_id, products[i], quantities[i], product_list.products[products[i]]->product_quantity);
            pthread_mutex_unlock(&console_lock);
        }
    }

    if (bought)
    { // if any successful bought happens when ordering some products
        pthread_mutex_lock(&console_lock);
        // to display the products bought by the customer successfully
        for (int i = 0; i < bought; i++)
            printf("Bought %d of product %d for %.2f\n", bought_quantities[i], bought_products[i], bought_quantities[i] * product_list.products[bought_products[i]]->product_price);

        // to display the products left in stock
        printf("\nUpdated Products:\n");
        printProductList(&product_list);
        pthread_mutex_unlock(&console_lock);
    }
}

// to check and buy the product if possible when customer order 1 product, SCENARIO 1
void purchase_product(Customer *customer, int product, int quantity)
{
    if (product_list.products[product]->product_quantity >= quantity)
    {
        if (customer->customer_balance >= (product_list.products[product]->product_price * quantity))
        {
            pthread_mutex_lock(&console_lock); // to lock the console for printing
            printf("\n");
            printf("Customer%d:\n", customer->customer_id);
            printf("Initial Products:\n");
            printProductList(&product_list);
            product_list.products[product]->product_quantity -= quantity;                             // decrease the number of items in stock
            customer->customer_balance -= (quantity * product_list.products[product]->product_price); // decrease the balance of the customer

            add_product_purchased(customer, product, quantity);
            printf("Bought %d of product %d for %.2f\n", quantity, product, quantity * product_list.products[product]->product_price);

            printf("\nUpdated Products:\n");
            printProductList(&product_list);
            printf("Customer%d(%d,%d) success! Paid %.2f for each.\n", customer->customer_id, product, quantity, product_list.products[product]->product_price);
            pthread_mutex_unlock(&console_lock);
        }
        else
        { // if the customer does not have enough money to buy the products with number of quantity suggested
            pthread_mutex_lock(&console_lock);
            printf("Customer%d(%d,%d) fail! Insufficient funds.\n", customer->customer_id, product, quantity);
            pthread_mutex_unlock(&console_lock);
        }
    }
    else
    { // if the suggested number of products do not exist in stock
        pthread_mutex_lock(&console_lock);
        printf("Customer%d(%d,%d) fail! Only %d left in stock\n", customer->customer_id, product, quantity, product_list.products[product]->product_quantity);
        pthread_mutex_unlock(&console_lock);
    }
}

// to order only one product at a time
void order_product(Customer *customer, int product_id, int quantity)
{
    // in spite of successfull or failed purchased it must be added to ordered list of the customer
    add_product_ordered(customer, product_id, quantity);

    pthread_mutex_lock(&list[product_id]); // to lock the product, so other threads waits until this customer unlocked
    purchase_product(customer, product_id, quantity);
    pthread_mutex_unlock(&list[product_id]); // unlock the product so other threads can now access to buy
}

void order_products(Customer *customer, int *product_ids, int *product_quantities, int number_of_products_ordered) // DID NOT WORK PROPERLY
{
    // to lock more than one product so other threads cannot access to change it
    for (int i = 0; i < number_of_products_ordered; i++)
        pthread_mutex_lock(&list[product_ids[i]]);

    // to send the ordered products to the function to check which one can be bought successfully
    purchase_products(customer, product_ids, product_quantities, number_of_products_ordered);

    // unlocking all products, so other threads can access and buy it
    for (int i = 0; i < number_of_products_ordered; i++)
        pthread_mutex_unlock(&list[product_ids[i]]);
}

// This method accepts the product_list and the product itself, and add it to the list
void addProductToList(ProductList *productList, Product *product)
{
    // allocate memory for new element
    pthread_mutex_lock(&mem_allocation_lock);
    Product **newProducts = (Product **)realloc(productList->products, (productList->size + 1) * sizeof(Product *));
    newProducts[productList->size] = product;

    // update product list size and products pointer
    productList->size++;
    productList->products = newProducts;
    pthread_mutex_unlock(&mem_allocation_lock);
}

// This method accepts the customer_list and the customer itself, and add it to the list
void addCustomerToList(CustomerList *customerList, Customer *customer)
{
    // allocate memory for new element
    pthread_mutex_lock(&mem_allocation_lock);
    Customer **newCustomers = (Customer **)realloc(customerList->customers, (customerList->size + 1) * sizeof(Customer *));

    newCustomers[customerList->size] = customer; // add new element to end of array

    // update product list size and products pointer
    customerList->size++;
    customerList->customers = newCustomers;
    pthread_mutex_unlock(&mem_allocation_lock);
}

// This method creates and returns a new product
Product *createProduct()
{
    // allocate memory for new product
    pthread_mutex_lock(&mem_allocation_lock);
    Product *newProduct = (Product *)malloc(sizeof(Product));
    // initialize product properties
    newProduct->product_id = product_list.size;
    newProduct->product_price = random_double(1.0, 200.0);
    newProduct->product_quantity = random_int(1, 10);
    pthread_mutex_unlock(&mem_allocation_lock);

    return newProduct;
}

// This method creates and returns a new customer
Customer *createCustomer()
{
    // allocate memory for new product
    pthread_mutex_lock(&mem_allocation_lock);
    Customer *newCustomer = (Customer *)malloc(sizeof(Customer));

    // initialize product properties
    newCustomer->customer_id = customer_list.size + 1;
    newCustomer->customer_balance = random_double(0.0, 200.0);
    newCustomer->initial_balance = newCustomer->customer_balance;
    newCustomer->ordered_items_size = 0;
    newCustomer->purchased_items_size = 0;
    newCustomer->ordered_items_max_size = 1;
    newCustomer->purchased_items_max_size = 1;
    pthread_mutex_unlock(&mem_allocation_lock);

    return newCustomer;
}

// Function for the customer thread
void *customer_thread(void *arg)
{
    int *i_ptr = (int *)arg; // cast the argument back to int pointer
    int i = *i_ptr;          // to know the thread points to which customer

    Customer *new_customer = customer_list.customers[i];

    if (SCENARIO == 1)
    {
        // SCENARIO 1: USE order_product() BELOW TO CHECK SCENARIO 1 and COMMENT EVERYTHING BELOW order_product() EXCEPT LAST LINE IN THIS FUNCTION
        int product_id = random_int(0, NUM_PRODUCTS);
        int quantity = random_int(1, 5);
        order_product(new_customer, product_id, quantity);
    }
    else
    {
        // SCENARIO 2: USE order_products() BELOW TO CHECK SCENARIO 2

        int number_of_products_ordered = random_int(2, NUM_PRODUCTS); // of course there must be at least 3 products to buy randomly
        int product_ids[number_of_products_ordered];
        int quantities[number_of_products_ordered];
        int in, im;
        im = 0;

        for (int i = 0; i < number_of_products_ordered; i++)
            quantities[i] = random_int(1, 5);

        for (in = 0; in < NUM_PRODUCTS && im < number_of_products_ordered; ++in)
        {
            int rn = NUM_PRODUCTS - in;
            int rm = number_of_products_ordered - im;
            if (rand() % rn < rm)
                product_ids[im++] = in;
        }
        order_products(new_customer, product_ids, quantities, number_of_products_ordered);
    }

    pthread_exit(NULL);
}

int main()
{
    product_list.size = 0;
    customer_list.size = 0;

    srand(time(NULL));
    int i;

    pthread_mutex_init(&console_lock, NULL);
    pthread_mutex_init(&mem_allocation_lock, NULL);
    for (i = 0; i < NUM_PRODUCTS; i++)
    {
        pthread_mutex_init(&list[i], NULL);
    }
    for (i = 0; i < NUM_PRODUCTS; i++)
    {
        Product *new_product = createProduct();
        addProductToList(&product_list, new_product);
    }

    printf("Products in Stock: \n");
    printProductList(&product_list); // to show the products in the beginning

    // Create the customer threads
    int j;

    // Create each Customer before giving it to any thread
    for (j = 0; j < NUM_CUSTOMERS; j++)
    {
        Customer *new_customer = createCustomer();
        addCustomerToList(&customer_list, new_customer);
    }

    pthread_t customer_threads[NUM_CUSTOMERS];
    for (j = 0; j < NUM_CUSTOMERS; j++)
    {
        int *i_ptr = malloc(sizeof(int));
        *i_ptr = j;
        if (pthread_create(&customer_threads[j], NULL, customer_thread, (void *)i_ptr))
            printf("Error generated while creating a Customer Thread\n");
    }

    for (j = 0; j < NUM_CUSTOMERS; j++)
    {
        if (pthread_join(customer_threads[j], NULL))
        {
            printf("Error while joining the thread\n");
            exit(-1);
        }
    }

    pthread_mutex_lock(&mem_allocation_lock);
    printCustomerList(&customer_list);
    pthread_mutex_unlock(&mem_allocation_lock);

    for (int i = 0; i < NUM_PRODUCTS; i++)
    {
        pthread_mutex_destroy(&list[i]);
    }

    pthread_mutex_destroy(&mem_allocation_lock);
    pthread_mutex_destroy(&console_lock);
    return 0;
}
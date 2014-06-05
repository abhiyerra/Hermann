// Hermann.c

/* Much of the librdkafka library calls were lifted from rdkafka_example.c */

#include "hermann_lib.h"

/**
 * Utility functions
 */

// Allocate a new Producer Configuration struct
HermannInstanceConfig newProducerConfig() {

}

// Allocate a new Consumer Configuration struct
HermannInstanceConfig newConsumerConfig() {
}

/**
 * Message delivery report callback.
 * Called once for each message.
 * See rdkafka.h for more information.
 */
static void msg_delivered (rd_kafka_t *rk,
			   void *payload, size_t len,
			   int error_code,
			   void *opaque, void *msg_opaque) {

	if (error_code)
		fprintf(stderr, "%% Message delivery failed: %s\n",
			rd_kafka_err2str(error_code));
	else if (!quiet)
		fprintf(stderr, "%% Message delivered (%zd bytes)\n", len);
}

static void hexdump (FILE *fp, const char *name, const void *ptr, size_t len) {
	const char *p = (const char *)ptr;
	int of = 0;


	if (name)
		fprintf(fp, "%s hexdump (%zd bytes):\n", name, len);

	for (of = 0 ; of < len ; of += 16) {
		char hexen[16*3+1];
		char charen[16+1];
		int hof = 0;

		int cof = 0;
		int i;

		for (i = of ; i < of + 16 && i < len ; i++) {
			hof += sprintf(hexen+hof, "%02x ", p[i] & 0xff);
			cof += sprintf(charen+cof, "%c",
				       isprint((int)p[i]) ? p[i] : '.');
		}
		fprintf(fp, "%08x: %-48s %-16s\n",
			of, hexen, charen);
	}
}

static void msg_consume (rd_kafka_message_t *rkmessage,
			 void *opaque) {
	if (rkmessage->err) {
		if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
			fprintf(stderr,
				"%% Consumer reached end of %s [%"PRId32"] "
			       "message queue at offset %"PRId64"\n",
			       rd_kafka_topic_name(rkmessage->rkt),
			       rkmessage->partition, rkmessage->offset);

			if (exit_eof)
				run = 0;

			return;
		}

		fprintf(stderr, "%% Consume error for topic \"%s\" [%"PRId32"] "
		       "offset %"PRId64": %s\n",
		       rd_kafka_topic_name(rkmessage->rkt),
		       rkmessage->partition,
		       rkmessage->offset,
		       rd_kafka_message_errstr(rkmessage));
		return;
	}

	if (!quiet)
		fprintf(stdout, "%% Message (offset %"PRId64", %zd bytes):\n",
			rkmessage->offset, rkmessage->len);

	if (rkmessage->key_len) {
		if (output == OUTPUT_HEXDUMP)
			hexdump(stdout, "Message Key",
				rkmessage->key, rkmessage->key_len);
		else
			printf("Key: %.*s\n",
			       (int)rkmessage->key_len, (char *)rkmessage->key);
	}

	if (output == OUTPUT_HEXDUMP)
		hexdump(stdout, "Message Payload",
			rkmessage->payload, rkmessage->len);
	else
		printf("%.*s\n",
		       (int)rkmessage->len, (char *)rkmessage->payload);

    // Yield the data to the Consumer's block
	if(rb_block_given_p()) {
	    fprintf(stderr, "Yield a value to block\n");
	    VALUE value = rb_str_new((char *)rkmessage->payload, rkmessage->len);
	    fprintf(stderr, "About to call yield\n");
	    rb_yield(value);
	} else {
	    fprintf(stderr, "No block given\n");
	}
}

static void sig_usr1 (int sig, rd_kafka_t *rk) {
	rd_kafka_dump(stdout, rk);
}

static void stop (int sig) {
	run = 0;
	fclose(stdin); /* abort fgets() */
}

/**
 * Kafka logger callback (optional)
 */
static void logger (const rd_kafka_t *rk, int level,
		    const char *fac, const char *buf) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	fprintf(stderr, "%u.%03u RDKAFKA-%i-%s: %s: %s\n",
		(int)tv.tv_sec, (int)(tv.tv_usec / 1000),
		level, fac, rd_kafka_name(rk), buf);
}

// Main entry point for Producer behavior
void actAsProducer(char* topic) {
}

// Main entry point for Consumer behavior
void actAsConsumer(VALUE self) {

    HermannInstanceConfig* consumerConfig;

    Data_Get_Struct(self, HermannInstanceConfig, consumerConfig);

    if(consumerConfig->topic==NULL) {
            fprintf(stderr, "Topic is null!");
            return;
    }
    fprintf(stderr, "actAsConsumer for topic %s\n", consumerConfig->topic);

   	quiet = !isatty(STDIN_FILENO);

    /* Kafka configuration */
	consumerConfig->conf = rd_kafka_conf_new();
	fprintf(stderr, "Kafka configuration created\n");

	/* Topic configuration */
	consumerConfig->topic_conf = rd_kafka_topic_conf_new();
	fprintf(stderr, "Topic configuration created\n");

	/* TODO: offset calculation */
	consumerConfig->start_offset = RD_KAFKA_OFFSET_END;

    signal(SIGINT, stop);
    signal(SIGUSR1, sig_usr1);
    fprintf(stderr, "Signals sent\n");

    /* Consumer specific code */

    fprintf(stderr, "Create a Kafka handle\n");
    /* Create Kafka handle */
    if (!(consumerConfig->rk = rd_kafka_new(RD_KAFKA_CONSUMER, consumerConfig->conf,
        consumerConfig->errstr, sizeof(consumerConfig->errstr)))) {
        fprintf(stderr, "%% Failed to create new consumer: %s\n", consumerConfig->errstr);
    	exit(1);
    }

    /* Set logger */
    /*rd_kafka_set_logger(rk, logger);
    fprintf(stderr, "Logger set\n");
    rd_kafka_set_log_level(rk, LOG_DEBUG);
    fprintf(stderr, "Loglevel configured\n");*/

    /* Add brokers */
    fprintf(stderr, "About to add brokers..");
    if (rd_kafka_brokers_add(consumerConfig->rk, consumerConfig->brokers) == 0) {
    fprintf(stderr, "%% No valid brokers specified\n");
    	exit(1);
    }
    fprintf(stderr, "Brokers added\n");

    /* Create topic */
    consumerConfig->rkt = rd_kafka_topic_new(consumerConfig->rk, consumerConfig->topic, consumerConfig->topic_conf);
    fprintf(stderr, "Topic created\n");

    /* Start consuming */
    if (rd_kafka_consume_start(consumerConfig->rkt, consumerConfig->partition, consumerConfig->start_offset) == -1){
        fprintf(stderr, "%% Failed to start consuming: %s\n",
            rd_kafka_err2str(rd_kafka_errno2err(errno)));
        exit(1);
    }

    fprintf(stderr, "Consume started\n");

    /* Run loop */
    while (run) {
        rd_kafka_message_t *rkmessage;

        /* Consume single message.
         * See rdkafka_performance.c for high speed
         * consuming of messages. */
        rkmessage = rd_kafka_consume(consumerConfig->rkt, consumerConfig->partition, 1000);
        if (!rkmessage) /* timeout */
            continue;

        msg_consume(rkmessage, NULL);

        /* Return message to rdkafka */
        rd_kafka_message_destroy(rkmessage);
    }

    fprintf(stderr, "Run loop exited\n");

    /* Stop consuming */
    rd_kafka_consume_stop(consumerConfig->rkt, consumerConfig->partition);

    rd_kafka_topic_destroy(consumerConfig->rkt);

    rd_kafka_destroy(consumerConfig->rk);

    /* todo: may or may not be necessary depending on how underlying threads are handled */
	/* Let background threads clean up and terminate cleanly. */
	rd_kafka_wait_destroyed(2000);
}

// Ruby gem extensions

/* Hermann::Consumer.consume */
static VALUE consume(VALUE self) {

    fprintf(stderr, "Consume invoked\n");
    actAsConsumer(self);

    return Qnil;
}

/* Hermann::Producer.push */
static VALUE push(VALUE self, VALUE message) {

    HermannInstanceConfig* producerConfig;

    Data_Get_Struct(self, HermannInstanceConfig, producerConfig);

    if(producerConfig->topic==NULL) {
        fprintf(stderr, "Topic is null!");
        return;
    }

   	quiet = !isatty(STDIN_FILENO);

    /* Kafka configuration */
	producerConfig->conf = rd_kafka_conf_new();
	fprintf(stderr, "Kafka configuration created\n");

	/* Topic configuration */
	producerConfig->topic_conf = rd_kafka_topic_conf_new();
	fprintf(stderr, "Topic configuration created\n");

    /* todo: This will need to be adapted to maintain the state through multiple invocations. */
    char buf[2048];
    char *msg = StringValueCStr(message);
    strcpy(buf, msg);

	/* Set up a message delivery report callback.
     * It will be called once for each message, either on successful
     * delivery to broker, or upon failure to deliver to broker. */
    rd_kafka_conf_set_dr_cb(producerConfig->conf, msg_delivered);

    /* Create Kafka handle */
    if (!(producerConfig->rk = rd_kafka_new(RD_KAFKA_PRODUCER, producerConfig->conf, producerConfig->errstr, sizeof(producerConfig->errstr)))) {
	    fprintf(stderr,
		"%% Failed to create new producer: %s\n", producerConfig->errstr);
        exit(1);
	}

    /* Set logger */
	rd_kafka_set_logger(producerConfig->rk, logger);
	rd_kafka_set_log_level(producerConfig->rk, LOG_DEBUG);

    if(rd_kafka_brokers_add(producerConfig->rk, producerConfig->brokers) == 0) {
			fprintf(stderr, "%% No valid brokers specified\n");
			exit(1);
	}

	/* Create topic */
	producerConfig->rkt = rd_kafka_topic_new(producerConfig->rk, producerConfig->topic, producerConfig->topic_conf);

    /* todo: copy message into buf */

    size_t len = strlen(buf);
    if (buf[len-1] == '\n')
        buf[--len] = '\0';

    /* Send/Produce message. */
	if (rd_kafka_produce(producerConfig->rkt, producerConfig->partition, RD_KAFKA_MSG_F_COPY,
        /* Payload and length */
		buf, len,
		/* Optional key and its length */
		NULL, 0,
		/* Message opaque, provided in
		* delivery report callback as
		* msg_opaque. */
		NULL) == -1) {

	    fprintf(stderr, "%% Failed to produce to topic %s partition %i: %s\n",
					rd_kafka_topic_name(producerConfig->rkt), producerConfig->partition,
					rd_kafka_err2str(rd_kafka_errno2err(errno)));

        /* Poll to handle delivery reports */
		rd_kafka_poll(producerConfig->rk, 0);
	 } else {
	    fprintf(stderr, "%% Sent %zd bytes to topic %s partition %i\n", len, rd_kafka_topic_name(producerConfig->rkt), producerConfig->partition);
	 }

    /* Wait for messages to be delivered */
    while (run && rd_kafka_outq_len(producerConfig->rk) > 0)
        rd_kafka_poll(producerConfig->rk, 100);

    /* Destroy the handle */
    rd_kafka_destroy(producerConfig->rk);

    /* todo: may or may not be necessary depending on how underlying threads are handled */
    /* Let background threads clean up and terminate cleanly. */
    rd_kafka_wait_destroyed(2000);
}

/* Hermann::Producer.batch { block } */
static VALUE batch(VALUE c) {
    /* todo: not implemented */
}

static void consumer_free(void * p) {
    /* todo: not implemented */
}

static VALUE consumer_allocate(VALUE klass) {

    VALUE obj;

    printf("consumer_allocate\n");
    HermannInstanceConfig* consumerConfig = ALLOC(HermannInstanceConfig);

    obj = Data_Wrap_Struct(klass, 0, consumer_free, consumerConfig);

    printf("consumer_allocate_end\n");

    return obj;
}

static VALUE consumer_initialize(VALUE self, VALUE topic) {

    HermannInstanceConfig* consumerConfig;
    char* topicPtr;

    printf("consumer_initialize\n");

    topicPtr = StringValuePtr(topic);
    fprintf(stderr, "Topic is:%s\n", topicPtr);

    Data_Get_Struct(self, HermannInstanceConfig, consumerConfig);

    /* todo: actually initialize the configuration options */
    consumerConfig->topic = topicPtr;
    consumerConfig->brokers = "localhost:9092";
    consumerConfig->partition = 0;

    printf("consumer_initialize_end\n");

    return self;
}

static VALUE consumer_init_copy(VALUE copy, VALUE orig) {
    HermannInstanceConfig* orig_config;
    HermannInstanceConfig* copy_config;

    if(copy == orig) {
        return copy;
    }

    if (TYPE(orig) != T_DATA || RDATA(orig)->dfree != (RUBY_DATA_FUNC)consumer_free) {
        rb_raise(rb_eTypeError, "wrong argument type");
    }

    Data_Get_Struct(orig, HermannInstanceConfig, orig_config);
    Data_Get_Struct(copy, HermannInstanceConfig, copy_config);

    // Copy over the data from one struct to the other
    MEMCPY(copy_config, orig_config, HermannInstanceConfig, 1);

    return copy;
}

static void producer_free(void * p) {
    /* todo: not implemented */
}

static VALUE producer_allocate(VALUE klass) {

    VALUE obj;

    printf("producer_allocate\n");
    HermannInstanceConfig* producerConfig = ALLOC(HermannInstanceConfig);

    obj = Data_Wrap_Struct(klass, 0, producer_free, producerConfig);

    printf("producer_allocate_end\n");

    return obj;
}

static VALUE producer_initialize(VALUE self, VALUE topic) {

    HermannInstanceConfig* producerConfig;
    char* topicPtr;

    printf("producer_initialize\n");

    topicPtr = StringValuePtr(topic);
    fprintf(stderr, "Topic is:%s\n", topicPtr);

    Data_Get_Struct(self, HermannInstanceConfig, producerConfig);

    /* todo: actually initialize the configuration options */
    producerConfig->topic = topicPtr;
    producerConfig->brokers = "localhost:9092";
    producerConfig->partition = 0;

    printf("producer_initialize_end\n");

    return self;
}

static VALUE producer_init_copy(VALUE copy, VALUE orig) {
    HermannInstanceConfig* orig_config;
    HermannInstanceConfig* copy_config;

    if(copy == orig) {
        return copy;
    }

    if (TYPE(orig) != T_DATA || RDATA(orig)->dfree != (RUBY_DATA_FUNC)producer_free) {
        rb_raise(rb_eTypeError, "wrong argument type");
    }

    Data_Get_Struct(orig, HermannInstanceConfig, orig_config);
    Data_Get_Struct(copy, HermannInstanceConfig, copy_config);

    // Copy over the data from one struct to the other
    MEMCPY(copy_config, orig_config, HermannInstanceConfig, 1);

    return copy;
}

void Init_hermann_lib() {

    /* Define the module */
    m_hermann = rb_define_module("Hermann");

    /* ---- Define the consumer class ---- */
    VALUE c_consumer = rb_define_class_under(m_hermann, "Consumer", rb_cObject);

    /* Allocate */
    rb_define_alloc_func(c_consumer, consumer_allocate);

    /* Initialize */
    rb_define_method(c_consumer, "initialize", consumer_initialize, 1);
    rb_define_method(c_consumer, "initialize_copy", consumer_init_copy, 1);

    /* Init Copy */
    /* todo: init copy for consumer */

    /* Consumer has method 'consume' */
    rb_define_method( c_consumer, "consume", consume, 0 );

    /* ---- Define the producer class ---- */
    VALUE c_producer = rb_define_class_under(m_hermann, "Producer", rb_cObject);

    /* Allocate */
    rb_define_alloc_func(c_producer, producer_allocate);

    /* Initialize */
    rb_define_method(c_producer, "initialize", producer_initialize, 1);
    rb_define_method(c_producer, "initialize_copy", producer_init_copy, 1);

    /* Init Copy */
    /* todo: init copy for consumer */

    /* Producer.push */
    rb_define_method( c_producer, "push", push, 1 );

    /* Producer.batch { block } */
    rb_define_method( c_producer, "batch", batch, 1 );
}
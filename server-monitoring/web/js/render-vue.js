var kpiTemp = new Vue({
    el: '#kpi-temp',
    data: {
        currentTemp: 'NA',
        minTemp: 'NA',
        maxTemp: 'NA'
    },
    methods: {
        loadData: function () {
            axios
                .get('/kpi-temp')
                .then(response => (
                    this.currentTemp = response.data.currentTemp,
                    this.minTemp = response.data.minTemp,
                    this.maxTemp = response.data.maxTemp
                ))
        }
    },
    mounted() {
        this.loadData();

        window.setInterval(() => { this.loadData() }, 60000);

    }
});

var graphTemp = new Vue({
    el: 'graphTemp',
    mounted() {
        var ctx = document.getElementById("graphTemp").getContext('2d');
        var temp_data = [];

        axios
            .get('/graph-temp')
            .then(function (response) {
                for (i = 0; i < response.data.temps.length; i++) {
                    temp_data.push(
                        {
                            t: new Date(response.data.temps[i].time),
                            y: response.data.temps[i].temp
                        }
                    );
                }

                var graphTemp = new Chart(ctx, {
                    type: 'line',
                    data: {
                        datasets: [{
                            label: 'Serre',
                            data: temp_data,
                            borderColor: 'rgba(75, 192, 192, 1)',
                            backgroundColor: 'rgba(75, 192, 192, 0.2)',
                            borderWidth: 1
                        }]
                    },
                    options: {
                        title: {
                            display: true,
                            text: 'Température des 7 derniers jours',
                            fontSize: 20,
                            fontFamily: '"Raleway", sans-serif'
                        },
                        scales: {
                            xAxes: [{
                                type: "time",
                                maxTicksLimit: 7,
                                time: {
                                    unit: 'day',
                                    tooltipFormat: "D MMM H:mm:ss",
                                    displayFormats: {
                                        hour: 'MMM D'
                                    }
                                }
                            }],
                        }
                    }
                });
            });
    }
});

var alert = new Vue({
    el: '#alert',
    data: {
        alertMsg: '',
        icon: {
            'fa-comment': false
        },
        color: {
            'w3-red': false,
            'w3-light-grey': true
        }
    },
    methods: {
        interpret: function (response) {
            if (response.data.status == "KO") {
                this.alertMsg = 'Alerte en cours: ' + response.data.desc;
                this.icon['fa-comment'] = true;
                this.color['w3-red'] = true;
                this.color['w3-light-grey'] = false;
            }
            else {
                this.alertMsg = '';
                this.icon['fa-comment'] = false;
                this.color['w3-red'] = false;
                this.color['w3-light-grey'] = true;
            }
        },
        loadData: function () {
            axios
                .get('/alert')
                .then(response => this.interpret(response));
        }
    },
    mounted() {
        this.loadData();

        window.setInterval(() => { this.loadData() }, 60000);
    }
});

var buzzer = new Vue({
    el: '#buzzer',
    data: {
        checkName: "Buzzer désactivé",
        isBuzzerActive: false
    },
    methods: {
        interpret: function (response) {
            if (response.data.status == "activated") {
                this.checkName = 'Buzzer activé';
                this.isBuzzerActive = true;
            }
            else {
                this.checkName = 'Buzzer désactivé';
                this.isBuzzerActive = false;
            }
        },
        checkBuzzer: function () {
            this.isBuzzerActive = !this.isBuzzerActive;

            if (this.isBuzzerActive) {
                this.checkName = 'Buzzer activé';

                axios
                    .post('/buzzer', {
                        status: 'on'
                    });
            }
            else {
                this.checkName = 'Buzzer désactivé';

                axios
                    .post('/buzzer', {
                        status: 'off'
                    });
            }
        }
    },
    mounted() {
        axios
            .get('/buzzer')
            .then(response => this.interpret(response));
    }
});

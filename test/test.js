'use strict'

let Plugin = require('node-plugin').Plugin;
let media = require('./media.js');


function notify(buf) {
    console.log(buf.toString());
}

let plugin = new Plugin('libwebstreamer');
let options = {
    plugin: {
        directory: __dirname + '/bin'
    },
    config: 'xxxxxx.json'
}
async function Initialize(arg) {
    await plugin.initialize(arg)
        .then(res => {
            console.log(res);
        })
        .catch(err => {
            console.log(err);
            throw new Error(err.toString());
        })
}

async function create_livestream(ipc_url) {
    let options = {
        rtspurl: ipc_url,
        videoCodec: 'h264',
        videoMode: 'sendonly',
        audioMode: 'sendonly',
        audioCodec: 'pcma',
        component: 'livestream1',
        protocol: 'rtspclient',
        endpoint: 'endpoint1',
        initiative: false
    }
    let buf = media.Create(options);
    await plugin.call(buf)
        .then(res => {
            console.log('=======>create_livestream successfully');
        })
        .catch(err => {
            throw new Error(err.toString());
        })
}
async function destroy_livestream() {
    let buf = media.Destroy('livestream1');
    await plugin.call(buf)
        .then(res => {
            console.log('=======>destroy_livestream successfully');
        })
        .catch(err => {
            throw new Error(err.toString());
        })
}
let rtsp_viewer1 = {
    rtspurl: '/test',//necessary
    protocol: 'rtspserver',//necessary
    component: 'livestream1',//necessary
    endpoint: 'endpoint2'//necessary
}
let rtsp_viewer2 = {
    rtspurl: '/test',//necessary
    protocol: 'rtspserver',//necessary
    component: 'livestream1',//necessary
    endpoint: 'endpoint3'//necessary
}
let test_viewer1 = {
    rtspurl: '/xxx',//necessary
    protocol: 'testsink',//necessary
    component: 'livestream1',//necessary
    endpoint: 'endpoint4'//necessary
}
async function add_viewer(options) {
    let buf = media.AddViewer(options);
    await plugin.call(buf)
        .then(res => {
            console.log('=======>add_viewer successfully');
        })
        .catch(err => {
            throw new Error(err.toString());
        })
}

async function remove_viewer(endpoint_id) {
    let buf = media.RemoveViewer('livestream1', endpoint_id);
    await plugin.call(buf)
        .then(res => {
            console.log('=======>remove_viewer successfully');
        })
        .catch(err => {
            throw new Error(err.toString());
        })
}


async function Terminate() {
    await plugin.terminate()
        .then(res => {
            console.log(res);
        })
        .catch(err => {
            console.log(err.toString());
            throw new Error(err.toString());
        })
}

async function test() {
    try {
        await Initialize(options);
        console.log('plugin version: ' + plugin.version());
        await create_livestream('rtsp://172.16.66.65/id=1');
        // await add_viewer(rtsp_viewer1);
        // await add_viewer(rtsp_viewer2);

        setInterval(() => {
            add_viewer(test_viewer1);
            setTimeout(() => {
                remove_viewer('endpoint4');
            }, 3000);
        }, 6000);

        // setTimeout(()=>{

        //     add_viewer(rtsp_viewer2);
        // },10000);
        // await remove_viewer('endpoint2');
        // await remove_viewer('endpoint1');
        // await destroy_livestream();
        // await Terminate();
    } catch (e) {
        console.log('-----------------error----------------');
        console.error(e.message);
        // await Terminate();
    }
}

plugin.on('notify', (buf) => {
    console.log('notify: ' + buf.toString());
})

test();
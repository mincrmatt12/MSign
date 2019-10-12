import React from 'react'
import Terminal from "./common/terminal"

class ConsolePane extends React.Component {
	constructor(props) {
		super(props);

		this.state = {
			texts: []
		}

		this.ws = null;
	}

	pushSystem(msg) {
		this.setState({
			texts: this.state.texts.concat([[1, [msg]]])
		});
	}

	pushResponse(msg) {
		let x = msg.split("\n");
		for (let i of x) this.pushSystem(i);
	}

	pushUser(msg) {
		this.setState({
			texts: this.state.texts.concat([[0, msg]])
		});
	}

	send(text) {
		if (this.ws == null || this.ws.readyState != WebSocket.OPEN) {
			this.pushSystem("Not connected!");
		}
		else {
			this.pushUser(text);
			this.ws.send(text);
		}
	}

	componentDidMount() {
		this.pushSystem("Connecting.");
		this.ws = new WebSocket("ws://msign:81");
		this.ws.onopen = () => {
			this.ws.send("d");
			this.pushSystem("Connected.");
		};
		this.ws.onmessage = (e) => {
			this.pushResponse(e.data);
		};
	}

	componentWillUnmount() {
		this.ws.close();
	}

	render() {
		return <div>
			<hr className="hr-gray" />
			<Terminal texts={this.state.texts} onSend={(text) => this.send(text)} />
			<hr className="hr-gray" />
		</div>
	}
}

class LogPane extends React.Component {
	constructor(props) {
		super(props);

		this.state = {
			texts: [],
			continue: false
		}

		this.ws = null;
	}

	pushSystem(t, msg) {
		this.setState({
			texts: this.state.texts.concat([[t, [msg]]])
		});
	}

	pushResponse(msg) {
		let x = msg.split("\n");
		if (this.state.continue) {
			this.setState((s) => {
				s.texts[s.texts.length - 1][1] += x[0];
				return s;
			});

			x = x.slice(1);
		}
		for (let i of x) this.pushSystem(1, i);
		if (msg[msg.length - 1] != "\n") {
			this.setState({continue: true});
		}
	}

	componentDidMount() {
		this.pushSystem(0, "Connecting.");
		this.ws = new WebSocket("ws://msign:81");
		this.ws.onopen = () => {
			this.ws.send("l");
			this.pushSystem(0, "Connected.");
		};
		this.ws.onmessage = (e) => {
			this.pushResponse(e.data);
		};
	}

	componentWillUnmount() {
		this.ws.close();
	}

	render() {
		return <div>
			<hr className="hr-gray" />
			<Terminal texts={this.state.texts} inputBar={false} />
			<hr className="hr-gray" />
		</div>
	}
}

export {
	ConsolePane,
	LogPane
};

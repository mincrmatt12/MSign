import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'

class Terminal extends React.Component {
	constructor(props) {
		super(props);

		this.state = {
			editText: ""
		};
		this.willScroll = false; // purposely not in state

		this.scrollRef = React.createRef();
	}

	onChangeText(e) {
		this.setState({editText: e.target.value});
	}

	shouldComponentUpdate() {
		if (this.scrollRef.scrollTop != undefined && this.scrollRef.scrollTop == this.scrollRef.scrollTopMax) {
			this.willScroll = true;
		}
		return true;
	}

	componentDidUpdate() {
		if (this.willScroll) {this.scrollRef.scrollTop = this.scrollRef.scrollTopMax;}
		this.willScroll = false;
	}

	render() {
		return <div>
			<div className="terminal" ref={(ref) => {this.scrollRef = ref}}>
				{this.props.texts.map((x) => {
					return <pre className={this.props.classNames[x[0]] + " terminal-text"}><output>{x[1]}</output></pre>
				})}
			</div>

			{this.props.inputBar && (
				<FormControl type="text rounded-0 rounded-bottom" onChange={(e) => this.onChangeText(e)} onKeyDown={(e) => {
					if (e.key == "Enter") {
						this.props.onSend(this.state.editText);
						this.setState({editText: ""});
					}
				}} value={this.state.editText} placeholder="enter command" />
			)}
		</div>
	}
}

Terminal.defaultProps = {
	classNames: ["text-success", "text-white"],
	inputBar: true,
	onSend: (text) => {console.log("unhooked: " + text);},
	texts: [[0, "test"], [1, "also test"], [1, "also test"]]
}

export default Terminal;
